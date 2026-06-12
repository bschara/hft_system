# HFT System

A modular, low-latency High-Frequency Trading system written in C++20, designed for real-time cryptocurrency market data processing, signal generation, and order execution across multiple venues and symbols.

## Overview

The system connects to exchange WebSocket feeds (Binance combined SBE streams), maintains per-symbol limit order books, generates trading signals via pluggable strategies, and routes trade intents through a risk-controlled position manager before sending orders via FIX protocol.

```
exchanges_data.csv  ──→  StreamConfig  ──→  VenueRegistry
                                                  │ instrument_id mapping
                              ┌───────────────────┘
                              │
             ┌────────────────▼─────────────────────┐
             │  MarketDataIngester × N venues        │
             │  (one thread per venue, SBE parser)   │
             └────────────────┬─────────────────────┘
                              │ LFQueue<MarketUpdate>
             ┌────────────────▼─────────────────────┐
             │         OrderBookManager              │
             │  routes by instrument_id              │
             │  ┌───────────────────────────────┐   │
             │  │ OrderBook[BTC]                │   │
             │  │ OrderBook[ETH]  × N symbols   │   │
             │  │ OrderBook[BNB]                │   │
             │  └───────────────────────────────┘   │
             └────────────────┬─────────────────────┘
                              │
             ┌────────────────▼─────────────────────┐
             │         StrategyManager               │
             │  per-symbol dispatch                  │
             │  ┌───────────────────────────────┐   │
             │  │ MidPriceReversion             │   │
             │  │ OrderBookImbalance  per symbol │   │
             │  │ MicroMomentum                 │   │
             │  └───────────────────────────────┘   │
             └────────────────┬─────────────────────┘
                              │ LFQueue<double>  signals ∈ [-1, 1]
             ┌────────────────▼─────────────────────┐
             │  PCModel  ←──  RiskModel, TCModel     │
             └────────────────┬─────────────────────┘
                              │ TradeIntent
             ┌────────────────▼─────────────────────┐
             │  OrderGateway (FIX / stunnel)         │
             └──────────────────────────────────────┘
```

## Features

- **Multi-venue, multi-symbol** — `VenueRegistry` maps `(exchange, symbol)` to a packed `uint32_t instrument_id`; adding a symbol requires only a new CSV row
- **Lock-free data pipeline** — `LFQueue<T>` SPSC ring buffers for zero-mutex cross-thread communication
- **Circular buffer order book** — 256-level LOB with O(1) update, modular-arithmetic indexing; zero-quantity removal and sparse-book protection built in
- **Per-symbol strategy dispatch** — `StrategyManager` routes each market update only to strategies registered for that instrument
- **SBE binary parser** — Decodes Binance WebSocket SBE (Simple Binary Encoding) depth streams; symbol extracted per-payload and stamped on every `MarketUpdate`
- **Transaction cost model** — Spread, slippage, and market impact estimation before order submission
- **Pre-trade risk checks** — Position limits, notional exposure, leverage, and trade size guards
- **Memory pool** — Pre-allocated object pool to avoid heap allocation on the hot path
- **FIX protocol gateway** — Ed25519-signed logon via stunnel proxy (in progress)

## Project Structure

```
hft_system/
├── CLAUDE.md                           # Claude Code context (project-wide)
├── exchanges_data.csv                  # Stream subscription config (source of truth for instruments)
├── fix_oe.conf                         # Stunnel FIX proxy config
├── CMakeLists.txt
├── include/
│   ├── market_data/
│   │   ├── CLAUDE.md                   # Market data module context
│   │   ├── venue_registry.hpp          # (exchange, symbol) → instrument_id mapping
│   │   ├── data_ingester/              # WebSocket SBE consumer (one per venue)
│   │   ├── historical_data_aggregator/
│   │   └── order_book/                 # LOB engine, MarketUpdate, OrderBookManager
│   ├── strategies/
│   │   ├── CLAUDE.md                   # Strategy module context
│   │   ├── strategy.h                  # Base interface
│   │   ├── strategy_manager.hpp        # Per-symbol dispatch
│   │   ├── mean_reversion/
│   │   └── trend_following/
│   └── utils/
│       ├── CLAUDE.md                   # Utils module context
│       ├── lock_free_queue.hpp         # SPSC ring buffer (header-only)
│       ├── memory_pool.hpp             # Pre-allocated pool (header-only)
│       ├── env_loader.hpp              # .env file loader (header-only)
│       ├── stream_config/              # CSV parser + URL builder
│       ├── websocket/
│       ├── tls_client/
│       ├── http_client/
│       ├── benchmark/
│       ├── macros.h
│       └── types.h
├── src/
│   ├── CLAUDE.md                       # Build conventions
│   ├── main.cpp                        # Entry point — wires all components
│   ├── market_data/
│   ├── strategies/
│   ├── utils/
│   ├── pcm_model/
│   ├── risk_management/
│   ├── tcm_model/
│   ├── order_gateway/
│   └── oms/
├── docs/                               # Extended documentation
└── tests/
    ├── CMakeLists.txt                  # FetchContent GoogleTest
    └── market_data/
        ├── CMakeLists.txt
        └── order_book_test.cpp         # 27 unit tests for OrderBook
```

## Dependencies

| Dependency         | Purpose                                |
| ------------------ | -------------------------------------- |
| OpenSSL            | TLS connections, Ed25519 signing, HMAC |
| libcurl            | HTTP REST client                       |
| simdjson           | High-performance JSON parsing          |
| PostgreSQL (libpq) | Historical tick data storage           |
| QuickFIX           | FIX protocol (submodule, in progress)  |
| vcpkg              | C++ package manager                    |
| Google Test        | Unit testing                           |

## Build

### Prerequisites

- CMake ≥ 3.10
- C++20-capable compiler (GCC 11+ or Clang 13+)
- vcpkg with `openssl`, `curl`, `simdjson`, `libpq` installed
- PostgreSQL development headers

### Steps

```bash
# 1. Install vcpkg packages
./vcpkg/bootstrap-vcpkg.sh
./vcpkg/vcpkg install openssl curl simdjson libpq

# 2. Configure
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_BUILD_TYPE=Release

# 3. Build
cmake --build build -j$(nproc)

# 4. Run
./build/MainExec
```

Use `Debug` for development (`ASSERT()`, address sanitizer). Prefer `Release` for latency benchmarking (`-O3`).

## Configuration

### Stream Subscriptions — `exchanges_data.csv`

The single source of truth for which instruments the system tracks. Adding a row is all that's needed to subscribe to a new symbol — no code changes required.

```csv
exchange,symbol,exchange_base_connection,stream
BINANCE,BTCUSDT,,btcusdt@depth
BINANCE,ETHUSDT,,ethusdt@depth
BINANCE,BNBUSDT,,bnbusdt@depth
```

`VenueRegistry` assigns each row a packed `instrument_id` at startup (venue index in bits [31:16], symbol index in bits [15:0]). `StreamConfig` builds the combined WebSocket URL automatically.

### API Credentials — `.env`

Copy and populate (never commit):

```bash
BINANCE_API_KEY=your_api_key
BINANCE_SECRET_KEY=-----BEGIN PRIVATE KEY-----
...Ed25519 PEM...
-----END PRIVATE KEY-----
KRAKEN_API_KEY=your_kraken_key
KRAKEN_SECRET_KEY=your_kraken_secret
```

### FIX Proxy — `fix_oe.conf`

Stunnel configuration for the Binance FIX order entry gateway:

```ini
[binance-fix]
client = yes
accept  = 127.0.0.1:6000
connect = fix-oe.binance.com:9000
```

Start stunnel before running the order gateway: `stunnel fix_oe.conf`

## Testing

Google Test is fetched automatically via CMake FetchContent — no manual install needed.

```bash
# Configure with tests (ON by default)
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON

cmake --build build -j$(nproc)

cd build && ctest --output-on-failure
```

Tests live under `tests/`. Current coverage:

| Suite | File | Tests |
|-------|------|-------|
| `OrderBook` | `tests/market_data/order_book_test.cpp` | 27 |

## Status

| Component                        | Status                                                    |
| -------------------------------- | --------------------------------------------------------- |
| TLS / WebSocket client           | Complete                                                  |
| VenueRegistry (multi-instrument) | Complete                                                  |
| Market data ingestion (SBE)      | Complete — symbol extracted per-payload, bugs fixed       |
| Circular buffer order book       | Complete — bugs fixed, 27 unit tests passing              |
| OrderBookManager (multi-symbol)  | Complete — dynamic routing by instrument_id               |
| Strategy framework + signals     | Complete — per-symbol dispatch                            |
| Mean reversion strategy          | Complete                                                  |
| Order book imbalance             | Complete                                                  |
| Micro-momentum                   | Complete                                                  |
| Transaction cost model           | Complete                                                  |
| Position/capital model           | Functional (simplified)                                   |
| Risk model                       | Interface defined, checks implemented                     |
| Historical data aggregator       | Partial (PostgreSQL connection + schema)                  |
| Order Management System          | Stub                                                      |
| FIX order gateway                | Partial (Ed25519 signing done, FIX session commented out) |
| Main entry point / thread wiring | Complete — all components wired, threads launched         |

## Docs

- [Architecture](docs/architecture.md) — Component roles, threading model, data flow
- [Market Data](docs/market_data.md) — Ingester, VenueRegistry, order book, SBE parsing
- [Strategies](docs/strategies.md) — Signal generation, per-symbol dispatch, strategy interface
- [Risk & Position](docs/risk_and_pcm.md) — TCM, risk checks, PCM sizing
- [Utilities](docs/utils.md) — Lock-free queue, memory pool, TLS, WebSocket
