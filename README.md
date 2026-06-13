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
                              │ LFQueue<MarketUpdate>  (int64 raw prices + exponent, no float)
             ┌────────────────▼─────────────────────┐
             │         OrderBookManager              │
             │  routes by instrument_id              │
             │  ┌───────────────────────────────┐   │
             │  │ OrderBook[BTC]                │   │
             │  │ OrderBook[ETH]  × N symbols   │   │
             │  │ OrderBook[BNB]  (int64 LOB)   │   │
             │  └───────────────────────────────┘   │
             └────────────────┬─────────────────────┘
                              │
             ┌────────────────▼─────────────────────┐
             │         StrategyManager               │
             │  per-symbol dispatch                  │
             │  ┌───────────────────────────────┐   │
             │  │ MidPriceReversion             │   │
             │  │ OrderBookImbalance  per symbol │   │
             │  │ MicroMomentum  (int arithmetic)│   │
             │  └───────────────────────────────┘   │
             └────────────────┬─────────────────────┘
                              │ LFQueue<int32_t>  signals ∈ [-1000, 1000]
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
- **Lock-free data pipeline** — `LFQueue<T>` SPSC ring buffers for zero-mutex cross-thread communication; one queue per venue (SPSC guarantee preserved)
- **Integer prices throughout** — `MarketUpdate` carries raw `int64_t` price/quantity + `int8_t` exponent; no float conversion until PCModel boundary; `PriceUtils::to_double()` for display only
- **Integer signals** — Strategies produce `int32_t` signals in `[-1000, 1000]`; pure integer arithmetic throughout (including `__int128` for price×qty products in OBI); PCModel is the single float boundary
- **Circular buffer order book** — 256-level LOB with O(1) update, integer tick-arithmetic indexing; zero-quantity removal and sparse-book protection built in
- **Schema-driven SBE decoder** — `MessageSchema` (pure byte-offset map) + `SchemaRegistry` (templateId → schema) + stateless `SBEDecoder`; `SBEVenueParser` outer loop handles multi-message frames; `VenueParser` interface makes adding new venues trivial
- **Per-symbol strategy dispatch** — `StrategyManager` routes each market update only to strategies registered for that instrument; three strategies active per symbol (MidPriceReversion, OrderBookImbalance, MicroMomentum), producing three signals per update
- **Pipeline latency measurement** — Per-stage nanosecond histograms (queue wait, book update, strategy dispatch, OBM total) using rdtsc; TSC calibrated to wall-clock at startup; p50/p99/p999 reported every 1M updates
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
│   │   ├── data_ingester/
│   │   │   ├── message_schema.h        # MessageSchema (byte-offset map) + kBinanceDepthV1
│   │   │   ├── schema_registry.h       # templateId → MessageSchema map
│   │   │   ├── venue_parser.h          # Abstract VenueParser interface
│   │   │   ├── sbe_decoder.h           # Stateless SBEDecoder (pure function, no float)
│   │   │   ├── sbe_venue_parser.h      # SBE dispatch shell (templateId → decode)
│   │   │   └── market_data_ingester.h  # TLS/WS recv loop — delegates to VenueParser
│   │   ├── historical_data_aggregator/
│   │   └── order_book/
│   │       ├── market_update.h         # int64 price/qty + int8 exponents
│   │       ├── order_book.h            # Circular LOB, int64 PriceLevel, getMidRaw()
│   │       └── order_book_manager.h    # Multi-venue routing + latency histograms
│   ├── strategies/
│   │   ├── CLAUDE.md                   # Strategy module context
│   │   ├── strategy.h                  # Base interface (returns int32_t [-1000,1000])
│   │   ├── strategy_manager.hpp        # Per-symbol dispatch, LFQueue<int32_t>
│   │   ├── mean_reversion/
│   │   └── trend_following/
│   └── utils/
│       ├── CLAUDE.md                   # Utils module context
│       ├── lock_free_queue.hpp         # SPSC ring buffer (header-only)
│       ├── memory_pool.hpp             # Pre-allocated pool (header-only)
│       ├── env_loader.hpp              # .env file loader (header-only)
│       ├── latency_tracker.hpp         # Log2 histogram + TSC calibration (header-only)
│       ├── price_utils.hpp             # PriceUtils::to_double() / to_string() — display only
│       ├── stream_config/              # CSV parser + URL builder
│       ├── websocket/
│       ├── tls_client/
│       ├── http_client/
│       ├── benchmark/                  # rdtsc_start / rdtsc_end fences
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
| Market data ingestion (SBE)      | Complete — schema-driven stateless decoder, multi-symbol frames, no float |
| Circular buffer order book       | Complete — int64 storage, integer tick math, 27 unit tests passing |
| OrderBookManager (multi-symbol)  | Complete — dynamic routing, per-stage rdtsc histograms    |
| Strategy framework + signals     | Complete — int32_t [-1000,1000] signals, LFQueue<int32_t> |
| Mean reversion strategy          | Complete — pure int64 arithmetic                          |
| Order book imbalance             | Complete — __int128 for price×qty products                |
| Micro-momentum                   | Complete — 20-tick window, int64 arithmetic                |
| Transaction cost model           | Complete                                                  |
| Position/capital model           | Functional (simplified)                                   |
| Risk model                       | Interface defined, checks implemented                     |
| Historical data aggregator       | Partial (PostgreSQL connection + schema)                  |
| Order Management System          | Stub                                                      |
| FIX order gateway                | Partial (Ed25519 signing done, FIX session commented out) |
| Main entry point / thread wiring | Complete — all components wired, threads launched         |
| Pipeline latency measurement     | Complete — per-stage rdtsc histograms, p50/p99/p999 output |

## Docs

- [Architecture](docs/architecture.md) — Component roles, threading model, data flow
- [Market Data](docs/market_data.md) — Ingester, VenueRegistry, order book, SBE parsing
- [Strategies](docs/strategies.md) — Signal generation, per-symbol dispatch, strategy interface
- [Risk & Position](docs/risk_and_pcm.md) — TCM, risk checks, PCM sizing
- [Utilities](docs/utils.md) — Lock-free queue, memory pool, TLS, WebSocket
