# HFT System

A modular, low-latency High-Frequency Trading system written in C++20, designed for real-time cryptocurrency market data processing, signal generation, and order execution on Binance.

## Overview

The system connects to Binance WebSocket streams, maintains live limit order books, generates trading signals via pluggable strategies, and routes trade intents through a risk-controlled position manager before sending orders via FIX protocol.

```
Binance WebSocket (TLS/SBE)
        ↓
MarketDataIngester
        ↓
LFQueue<MarketUpdate>
        ↓
OrderBookManager  ──→  OrderBook × N symbols
        ↓
StrategyManager
  ├── MidPriceReversion
  ├── OrderBookImbalance
  └── MicroMomentum
        ↓
LFQueue<double> (signals)
        ↓
PCModel  ←── RiskModel, TCModel
        ↓
TradeIntent → OrderGateway (FIX/REST) → Exchange
```

## Features

- **Lock-free data pipeline** — `LFQueue<T>` ring buffers for zero-mutex cross-thread communication
- **Circular buffer order book** — 256-level LOB with O(1) update, modular-arithmetic indexing
- **Pluggable strategies** — Register any `Strategy` subclass; all receive each market update
- **Transaction cost model** — Spread, slippage, and market impact estimation before order submission
- **Pre-trade risk checks** — Position limits, notional exposure, leverage, and trade size guards
- **SBE binary parser** — Decodes Binance WebSocket SBE (Simple Binary Encoding) depth stream
- **Memory pool** — Pre-allocated object pool to avoid heap allocation on the hot path
- **FIX protocol gateway** — Ed25519-signed logon via stunnel proxy (in progress)

## Project Structure

```
hft_system/
├── include/                        # Public headers (mirrored by src/)
│   ├── market_data/
│   │   ├── data_ingester/          # WebSocket market data consumer
│   │   ├── historical_data_aggregator/
│   │   └── order_book/             # LOB engine + update types
│   ├── strategies/
│   │   ├── mean_reversion/
│   │   └── trend_following/
│   ├── pcm_model/                  # Position/Capital Management
│   ├── tcm_model/                  # Transaction Cost Model
│   ├── risk_management/
│   ├── oms/                        # Order Management System
│   ├── order_gateway/              # FIX order gateway
│   └── utils/
│       ├── websocket/
│       ├── tls_client/
│       ├── http_client/
│       ├── stream_config/
│       ├── benchmark/
│       ├── lock_free_queue.hpp
│       ├── memory_pool.hpp
│       ├── env_loader.hpp
│       ├── macros.h
│       └── types.h
├── src/                            # Implementations
├── tests/                          # Google Test unit tests
├── exchanges_data.csv              # Stream subscription config
├── fix_oe.conf                     # Stunnel FIX proxy config
├── CMakeLists.txt
└── docs/                           # Extended documentation
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

### Build Type

For latency-sensitive work, prefer `Release` (`-O3`, no assertions):

```bash
-DCMAKE_BUILD_TYPE=Release
```

Use `Debug` for development to enable `ASSERT()` and address sanitizer.

## Configuration

### Stream Subscriptions — `exchanges_data.csv`

Controls which exchange symbols and WebSocket streams to subscribe to:

```csv
exchange,symbol,exchange_base_connection,stream
BINANCE,BTCUSDT,,btcusdt@depth
BINANCE,ETHUSDT,,ethusdt@depth
BINANCE,BNBUSDT,,bnbusdt@depth
```

The `stream` column maps directly to Binance stream names. `StreamConfig` builds the combined WebSocket URL automatically.

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

```bash
cd build
ctest --output-on-failure
```

Tests use Google Test and are located under `tests/`.

## Status

| Component                        | Status                                                    |
| -------------------------------- | --------------------------------------------------------- |
| TLS / WebSocket client           | Complete                                                  |
| Market data ingestion (SBE)      | Working (minor bug in ask-level loop)                     |
| Circular buffer order book       | Complete                                                  |
| Order book manager               | Complete                                                  |
| Strategy framework + signals     | Complete                                                  |
| Mean reversion strategy          | Complete                                                  |
| Order book imbalance             | Complete                                                  |
| Micro-momentum                   | Complete                                                  |
| Transaction cost model           | Complete                                                  |
| Position/capital model           | Functional (simplified)                                   |
| Risk model                       | Interface defined, checks implemented                     |
| Historical data aggregator       | Partial (PostgreSQL connection + schema)                  |
| Order Management System          | Stub                                                      |
| FIX order gateway                | Partial (Ed25519 signing done, FIX session commented out) |
| Main entry point / thread wiring | In progress                                               |

## Docs

- [Architecture](docs/architecture.md) — Component roles, threading model, data flow
- [Market Data](docs/market_data.md) — Ingester, order book, SBE parsing
- [Strategies](docs/strategies.md) — Signal generation, strategy interface
- [Risk & Position](docs/risk_and_pcm.md) — TCM, risk checks, PCM sizing
- [Utilities](docs/utils.md) — Lock-free queue, memory pool, TLS, WebSocket
