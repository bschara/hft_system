# Architecture

## Component Overview

The system is decomposed into independently-compiled C++ libraries, each responsible for a single concern. The main executable links them all and wires up the data pipeline.

```
┌──────────────────────────────────────────────────────────────────────┐
│                   EXCHANGE(S)  (Binance, Kraken, ...)                │
│              WebSocket (SBE binary depth stream)                     │
└────────────────────────────┬─────────────────────────────────────────┘
                             │ TLS (OpenSSL) — one connection per venue
              ┌──────────────▼──────────────┐
              │     MarketDataIngester       │  recv loop only
              │     × N venues              │  delegates to VenueParser
              │     (one thread each)       │
              └──────────────┬──────────────┘
                             │  VenueParser::parse()
              ┌──────────────▼──────────────┐
              │  SBEVenueParser              │  templateId → schema lookup
              │  SBEDecoder (stateless)      │  reads raw int64 prices
              │  SchemaRegistry              │  no float conversion
              └──────────────┬──────────────┘
                             │ LFQueue<MarketUpdate>  (int64 raw prices + exponent)
              ┌──────────────▼──────────────┐
              │      OrderBookManager       │  routes by instrument_id
              │                             │  (packed uint32_t)
              │  ┌────────────────────────┐ │
              │  │ OrderBook[BTC/BINANCE] │ │  circular buffer LOB
              │  │ OrderBook[ETH/BINANCE] │ │  256 levels, int64 storage
              │  │ OrderBook[BNB/BINANCE] │ │  O(1) update
              │  └────────────────────────┘ │
              └──────────────┬──────────────┘
                             │
              ┌──────────────▼──────────────┐
              │      StrategyManager        │  per-symbol dispatch
              │                             │
              │  ┌────────────────────────┐ │
              │  │ MidPriceReversion[BTC] │ │  pure int64 math
              │  │ OrderBookImbalance[ETH]│ │  __int128 for products
              │  │ MicroMomentum[BNB]     │ │  int64 window counter
              │  └────────────────────────┘ │
              └──────────────┬──────────────┘
                             │ LFQueue<int32_t>  signals ∈ [-1000, 1000]
              ┌──────────────▼──────────────┐
              │           PCModel           │  signal / 1000 → strength
              │                             │  (only float conversion)
              │  ┌────────────────────────┐ │
              │  │  RiskModel             │ │  pre-trade risk checks
              │  │  TCModel               │ │  cost estimation
              │  └────────────────────────┘ │
              └──────────────┬──────────────┘
                             │ TradeIntent
              ┌──────────────▼──────────────┐
              │    OrderGateway (FIX)        │  Ed25519 logon signing
              │    stunnel → Exchange        │  (in progress)
              └─────────────────────────────┘
```

## Instrument Identity

Every market event is tagged with a `uint32_t instrument_id`:

```
bits [31:16]  venue_idx   (0 = BINANCE, 1 = KRAKEN, ...)
bits [15:0]   symbol_idx  (0 = BTCUSDT, 1 = ETHUSDT, ...)
```

`VenueRegistry` (`include/market_data/venue_registry.hpp`) is the single source of truth. It is built at startup from `exchanges_data.csv` rows and is read-only thereafter. `SBEDecoder` calls `registry.lookup(venue, symbol)` once per WebSocket payload to get the `instrument_id`, then stamps it on every `MarketUpdate` in that payload.

## Threading Model

| Thread | Responsibility |
|--------|----------------|
| Ingester thread × N | TLS recv loop → `VenueParser::parse()` → enqueue `MarketUpdate` |
| OBM thread | Dequeue `MarketUpdate` → update `OrderBook` → dispatch to `StrategyManager` |
| Strategy/PCM thread | Consume signals → generate `TradeIntent` → risk check (not yet wired) |
| Gateway thread | Send orders via FIX session (not yet wired) |

All inter-thread communication via `LFQueue<T>` (SPSC, lock-free). No mutexes on the hot path. Each venue gets its own dedicated `LFQueue<MarketUpdate>` — Binance combined streams multiplex all symbols over a single WebSocket, so one ingester per venue suffices.

## Memory Model

- **`LFQueue<T>`** — ring buffer, capacity set at construction, heap-allocated once
- **`MemPool<T>`** — pre-allocated object pool; zero heap allocation after construction
- **`OrderBook`** — `std::array<PriceLevel, 256>` per side; no heap after construction; stored in a `std::vector` in `OrderBookManager` that is `reserve()`d at startup and never resized
- **`MarketUpdate`** — `alignas(64)`, one cache line; carries raw `int64_t` price/qty, `int8_t` exponents, and `_recv_tsc`

## Key Design Decisions

### No Float on the Hot Path

Prices and quantities are carried as raw `int64_t` + `int8_t` exponents all the way from the SBE decoder through the order book and into every strategy. The only float conversion in the entire pipeline is in `PCModel::generatetradeIntent()` where `int32_t signal / 1000.0` computes position strength. `PriceUtils::to_double(raw, exp)` is available for logging and display but is never called on the hot path.

### Integer Signals: `[-1000, 1000]`

Strategies return `int32_t` instead of `double`. The convention `1000 = maximum conviction` maps cleanly to the old `1.0` without any float arithmetic inside strategies. `OrderBookImbalance` uses `__int128` for intermediate `price × qty` products to prevent int64 overflow on large raw integers.

### Instrument Identity: Packed `uint32_t`

A single `uint32_t` encodes both venue and symbol in a form that fits in one register. The OBM hot path is:
```
instrument_id → unordered_map lookup → array index → OrderBook::addUpdate()
```
No string comparisons, no allocations. `VenueRegistry` handles the string-to-id mapping at parse time (once per payload), not dispatch time (once per price level).

### Schema-Driven Stateless Decoder

The ingestion layer is split into three concerns:
- **`MessageSchema`** — a pure data struct describing field byte-offsets, encoding types, and timestamp units for one SBE templateId. Defined at compile time (`constexpr kBinanceDepthV1`).
- **`SBEDecoder::decode()`** — a stateless pure function. Takes a `std::span<const uint8_t>` and a `MessageSchema`, reads raw integers from the wire, and enqueues `MarketUpdate` objects. Returns bytes consumed; the caller advances the cursor.
- **`SBEVenueParser`** — outer loop: reads `templateId` from the SBE header, looks up the schema in `SchemaRegistry`, calls `SBEDecoder::decode()`, advances the cursor. Handles frames containing multiple SBE messages.

Adding a new venue requires only subclassing `VenueParser` (or registering a new schema in `SchemaRegistry`) — `MarketDataIngester` is unchanged.

### Stable OrderBook References

`OrderBookManager` stores `OrderBook` objects in a `std::vector` that is `reserve()`d with the full instrument count at construction, then populated via `register_instrument()` before any threads launch. Because the vector is never resized after that point, references returned by `book_for()` are stable for the lifetime of the process.

### Circular Buffer Order Book (Integer Indexing)

The LOB uses a circular array of `PriceLevel` per side, indexed relative to the current best price using integer tick arithmetic:
```
index = (bestIndex + (price - bestPrice) / TICK_UNITS + LOB_DEPTH) % LOB_DEPTH
```
`TICK_UNITS = 1` (one raw integer unit = one minimum price increment). Higher index = higher price for both sides. `bestBidIndex` holds the highest bid; `bestAskIndex` holds the lowest ask. Updates more than `MAX_SHIFT_STEP` (64) raw ticks outside the current window are silently dropped. This gives O(1) update and lookup at the cost of a fixed price range window (256 ticks).

### Lock-Free Queue (SPSC) — Per-Venue Design

`LFQueue<T>` works without locks because exactly one thread writes and one thread reads. Each venue gets its own dedicated `LFQueue<MarketUpdate>`, registered with the OBM via `add_queue()`. The OBM round-robins over all registered queues in `run()`. This preserves the SPSC guarantee regardless of how many venues are active.

### Per-Symbol Strategy Dispatch

`StrategyManager` maintains a `std::unordered_map<uint32_t, std::vector<Strategy*>>` keyed by `instrument_id`. On each update it dispatches only to strategies registered for that instrument. Catch-all strategies (registered without an instrument_id) receive all updates.

Currently three strategies are registered per symbol (BTC, ETH, BNB): `MidPriceReversion`, `OrderBookImbalance`, and `MicroMomentum`. Each `MarketUpdate` produces three `int32_t` signals written to `LFQueue<int32_t>` (capacity 8192).

## Module Dependency Graph

```
MainExec
├── MarketDataIngester
│   ├── SBEVenueParser
│   │   ├── SBEDecoder
│   │   │   └── MessageSchema / SchemaRegistry (header-only)
│   │   └── VenueRegistry (header-only)
│   ├── WebSocket
│   │   └── TLSClient (OpenSSL)
│   └── LFQueue (header-only)
├── OrderBook  [contains OrderBookManager]
│   └── OrderBook (int64 circular buffer LOB)
├── StrategyManager (header-only)
│   ├── MidPriceReversion
│   ├── OrderBookImbalance
│   └── MicroMomentum
├── PCModel
│   ├── RiskModel (header-only)
│   └── TCModel
├── OrderGateway (FIX — in progress)
├── OrderManagementSystem (stub)
├── HistoricalDataAggregator
│   ├── HttpClient (libcurl)
│   └── PostgreSQL (libpq)
└── StreamConfig
    └── CSVReader (csv.h — header-only)
```

## Latency Measurement

Pipeline latency is measured in the OBM thread using `rdtsc` fences (no locks, no allocations). Four stages are tracked per update:

| Histogram | Measured interval |
|-----------|------------------|
| `queue_wait` | `dequeue_tsc − update._recv_tsc` — time the update sat in `LFQueue` |
| `book_update` | `rdtsc` around `OrderBook::addUpdate()` |
| `strategy` | `rdtsc` around `StrategyManager::onMarketData()` |
| `obm_total` | Full OBM processing time (dequeue → end of strategy dispatch) |

Each histogram is a 64-bucket log₂ accumulator (`LatencyHistogram` in `include/utils/perf/latency_tracker.hpp`). Every 1,000,000 updates, `OrderBookManager::report_latencies()` prints p50/p99/p999 to stdout. TSC frequency is calibrated against `CLOCK_MONOTONIC` during a 10 ms spin at startup (`Common::calibrate_tsc_ns()`).

The ingester stamps `_recv_tsc = rdtsc_start()` once per SBE payload before enqueuing any level — all price levels in a payload share the same ingestion timestamp.

## File Conventions

- Headers in `include/<module>/`, implementations in `src/<module>/`
- Each module has its own `CMakeLists.txt` producing a static library
- Header-only utilities: `containers/lock_free_queue.hpp`, `containers/memory_pool.hpp`, `venue_registry.hpp`, `strategy_manager.hpp`, `config/env_loader.hpp`, `pricing/price_utils.hpp`
- `.env` is loaded at startup via `loadEnv()` and must not be committed
- `exchanges_data.csv` is the single config file for adding instruments — no code changes needed
