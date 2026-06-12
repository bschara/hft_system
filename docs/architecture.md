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
              │     MarketDataIngester       │  parses SBE frames
              │     × N venues              │  looks up instrument_id
              │     (one thread each)       │  via VenueRegistry
              └──────────────┬──────────────┘
                             │ LFQueue<MarketUpdate>  (shared, all venues)
              ┌──────────────▼──────────────┐
              │      OrderBookManager       │  routes by instrument_id
              │                             │  (packed uint32_t)
              │  ┌────────────────────────┐ │
              │  │ OrderBook[BTC/BINANCE] │ │  circular buffer LOB
              │  │ OrderBook[ETH/BINANCE] │ │  256 levels per side
              │  │ OrderBook[BNB/BINANCE] │ │  O(1) update
              │  └────────────────────────┘ │
              └──────────────┬──────────────┘
                             │
              ┌──────────────▼──────────────┐
              │      StrategyManager        │  per-symbol dispatch
              │                             │
              │  ┌────────────────────────┐ │
              │  │ MidPriceReversion[BTC] │ │  mean reversion signal
              │  │ OrderBookImbalance[ETH]│ │  bid/ask imbalance
              │  │ MicroMomentum[BNB]     │ │  aggressive order flow
              │  └────────────────────────┘ │
              └──────────────┬──────────────┘
                             │ LFQueue<double>  signals ∈ [-1, 1]
              ┌──────────────▼──────────────┐
              │           PCModel           │  signal → TradeIntent
              │                             │  capital_fraction = 0.02
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

`VenueRegistry` (`include/market_data/venue_registry.hpp`) is the single source of truth. It is built at startup from `exchanges_data.csv` rows and is read-only thereafter. The ingester calls `registry.lookup(venue, symbol)` once per WebSocket payload to get the `instrument_id`, then stamps it on every `MarketUpdate` in that payload.

## Threading Model

| Thread | Responsibility |
|--------|----------------|
| Ingester thread × N | TLS recv loop → SBE parse → `registry.lookup()` → enqueue `MarketUpdate` |
| OBM thread | Dequeue `MarketUpdate` → update `OrderBook` → dispatch to `StrategyManager` |
| Strategy/PCM thread | Consume signals → generate `TradeIntent` → risk check (not yet wired) |
| Gateway thread | Send orders via FIX session (not yet wired) |

All inter-thread communication via `LFQueue<T>` (SPSC, lock-free). No mutexes on the hot path. All ingesters share one `LFQueue<MarketUpdate>` — Binance combined streams multiplex all symbols over a single WebSocket, so one ingester per venue suffices.

## Memory Model

- **`LFQueue<T>`** — ring buffer, capacity set at construction, heap-allocated once
- **`MemPool<T>`** — pre-allocated object pool; zero heap allocation after construction
- **`OrderBook`** — `std::array<PriceLevel, 256>` per side; no heap after construction; stored in a `std::vector` in `OrderBookManager` that is `reserve()`d at startup and never resized
- **`MarketUpdate`** — `alignas(32)`, fits in one cache line

## Key Design Decisions

### Instrument Identity: Packed `uint32_t`

A single `uint32_t` encodes both venue and symbol in a form that fits in one register. The OBM hot path is:
```
instrument_id → unordered_map lookup → array index → OrderBook::addUpdate()
```
No string comparisons, no allocations. `VenueRegistry` handles the string-to-id mapping at parse time (once per payload), not dispatch time (once per price level).

### Stable OrderBook References

`OrderBookManager` stores `OrderBook` objects in a `std::vector` that is `reserve()`d with the full instrument count at construction, then populated via `register_instrument()` before any threads launch. Because the vector is never resized after that point, references returned by `book_for()` are stable for the lifetime of the process.

### Circular Buffer Order Book

The LOB uses a circular array of `PriceLevel` per side, indexed relative to the current best price:
```
index = (bestIndex + round((price - bestPrice) / TICK_SIZE) + LOB_DEPTH) % LOB_DEPTH
```
Higher index = higher price for both sides. `bestBidIndex` holds the highest bid; `bestAskIndex` holds the lowest ask. Updates more than `MAX_SHIFT_STEP` (64) ticks outside the current window are silently dropped to prevent index overflow. This gives O(1) update and lookup at the cost of a fixed price range window (256 × tick size). Zero-quantity updates delete a level in-place; the best/worst endpoint is updated by scanning for the next active slot.

### Lock-Free Queue (SPSC)

`LFQueue<T>` works without locks because exactly one thread writes and one thread reads. Atomic indices prevent torn reads. Adding a second producer (second venue ingester) requires either a dedicated queue per venue or an MPSC implementation — currently all ingesters share one queue, which is safe because the Binance combined-stream model means exactly one ingester thread.

### Per-Symbol Strategy Dispatch

`StrategyManager` maintains a `std::unordered_map<uint32_t, std::vector<Strategy*>>` keyed by `instrument_id`. On each update it dispatches only to strategies registered for that instrument. Catch-all strategies (registered without an instrument_id) receive all updates. This eliminates per-update branching inside strategies.

### Signal Normalization

All strategies produce signals in `[-1, 1]`. `PCModel` maps signal strength to a fraction of available capital (`capital_fraction = 0.02` per trade by default).

## Module Dependency Graph

```
MainExec
├── MarketDataIngester
│   ├── WebSocket
│   │   └── TLSClient (OpenSSL)
│   ├── VenueRegistry (header-only)
│   └── LFQueue (header-only)
├── OrderBook  [contains OrderBookManager]
│   └── OrderBook (circular buffer LOB)
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

## File Conventions

- Headers in `include/<module>/`, implementations in `src/<module>/`
- Each module has its own `CMakeLists.txt` producing a static library
- Header-only utilities: `lock_free_queue.hpp`, `memory_pool.hpp`, `venue_registry.hpp`, `strategy_manager.hpp`, `env_loader.hpp`
- `.env` is loaded at startup via `loadEnv()` and must not be committed
- `exchanges_data.csv` is the single config file for adding instruments — no code changes needed
