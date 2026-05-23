# Architecture

## Component Overview

The system is decomposed into 11 independently-compiled libraries, each responsible for a single concern. The main executable links them all and wires up the data pipeline.

```
┌─────────────────────────────────────────────────────────────────┐
│                        EXCHANGE (Binance)                       │
│              WebSocket (SBE binary depth stream)                │
└───────────────────────────┬─────────────────────────────────────┘
                            │ TLS (OpenSSL)
                ┌───────────▼────────────┐
                │   MarketDataIngester   │  parses SBE frames,
                │   (TLSClient +         │  produces MarketUpdate
                │    WebSocket)          │  structs
                └───────────┬────────────┘
                            │ LFQueue<MarketUpdate>
                ┌───────────▼────────────┐
                │   OrderBookManager     │  routes by symbol_id,
                │                        │  maintains N OrderBooks
                │   ┌──────────────────┐ │
                │   │ OrderBook[BTC]   │ │  circular buffer LOB
                │   │ OrderBook[ETH]   │ │  256 levels per side
                │   │ OrderBook[BNB]   │ │
                │   └──────────────────┘ │
                └───────────┬────────────┘
                            │
                ┌───────────▼────────────┐
                │   StrategyManager      │  dispatches MarketUpdate
                │                        │  to all registered strats
                │   ┌──────────────────┐ │
                │   │MidPriceReversion │ │  mean reversion signal
                │   │OrderBookImbalance│ │  bid/ask imbalance
                │   │MicroMomentum     │ │  aggressive order flow
                │   └──────────────────┘ │
                └───────────┬────────────┘
                            │ LFQueue<double> (signals ∈ [-1, 1])
                ┌───────────▼────────────┐
                │       PCModel          │  signal → TradeIntent
                │                        │  capital_fraction=0.02
                │   ┌──────────────────┐ │
                │   │  RiskModel       │ │  pre-trade risk checks
                │   │  TCModel         │ │  cost estimation
                │   └──────────────────┘ │
                └───────────┬────────────┘
                            │ TradeIntent
                ┌───────────▼────────────┐
                │  OrderGateway (FIX)    │  Ed25519 logon signing
                │  stunnel → Binance     │  (in progress)
                └────────────────────────┘
```

## Threading Model

The design assumes each stage runs in its own thread, communicating through lock-free queues:

| Thread | Responsibility |
|---|---|
| Ingester thread | TLS recv loop → SBE parse → enqueue `MarketUpdate` |
| Book thread | Dequeue `MarketUpdate` → update `OrderBook` → dispatch to strategies |
| Strategy/PCM thread | Consume signals → generate `TradeIntent` → risk check |
| Gateway thread | Send orders via FIX session |

No mutexes are used in the hot path. `LFQueue<T>` uses `std::atomic` indices (single-producer / single-consumer).

Thread management (`std::thread` creation, pinning to CPU cores) is not yet wired in `main.cpp`.

## Memory Model

- **`LFQueue<T>`** — ring buffer, compile-time capacity, heap-allocated once at startup
- **`MemPool<T>`** — pre-allocated object pool; `allocate()` returns a slot, `deallocate()` marks it free
- **`OrderBook`** — `std::array<PriceLevel, 256>` on each side; no heap allocation after construction
- **`Order`** — 64-byte aligned struct to fit one cache line

## Key Design Decisions

### Circular Buffer Order Book

The LOB uses a circular array of `PriceLevel` indexed by `(price / TICK_SIZE) % LOB_DEPTH`. When prices move outside the buffer window, `shiftBookToPrice()` clears stale levels and updates the base reference price. This gives O(1) update and lookup at the cost of a fixed price range window (256 × tick size).

### Lock-Free Queue (Single-Producer / Single-Consumer)

`LFQueue<T>` works without locks because only one thread writes (`updateWriteIndex`) and one thread reads (`updateReadIndex`). The atomic indices prevent torn reads but impose a memory fence on each operation. This is acceptable for inter-thread handoff at the queue boundary.

### Strategy Abstraction

`Strategy` is a pure virtual base class with one method: `onMarketData(const MarketUpdate*)`. `StrategyManager` holds up to 10 strategy pointers and calls each on every update. Adding a strategy requires only subclassing `Strategy` and calling `register_strategy()` — no changes elsewhere.

### Signal Normalization

All strategies produce signals in `[-1, 1]`:
- `+1` = maximum buy conviction
- `-1` = maximum sell conviction
- `0` = flat / no edge

`PCModel` maps signal strength to a fraction of available capital (`capital_fraction = 0.02` per trade by default).

## Module Dependency Graph

```
MainExec
├── MarketDataIngester
│   ├── WebSocket
│   │   └── TLSClient (OpenSSL)
│   └── (LFQueue — header only)
├── OrderBookManager
│   └── OrderBook
├── StrategyManager
│   ├── MidPriceReversion
│   ├── OrderBookImbalance
│   └── MicroMomentum
├── PCModel
│   ├── RiskModel (header only)
│   └── TCModel
├── OrderGateway (FIX — in progress)
├── OrderManagementSystem (stub)
├── HistoricalDataAggregator
│   ├── HttpClient (libcurl)
│   └── PostgreSQL (libpq)
└── StreamConfig
    └── CSVReader (csv.h — header only)
```

## File Conventions

- Headers live in `include/<module>/`, implementations in `src/<module>/`
- Each module has its own `CMakeLists.txt` producing a static library
- `include/utils/` contains header-only utilities (`lock_free_queue.hpp`, `memory_pool.hpp`, `macros.h`, `csv.h`)
- `.env` is loaded at startup via `loadEnv()` and must not be committed
