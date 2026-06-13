# HFT System — Claude Context

## What This Project Is

A C++20 high-frequency trading engine for cryptocurrency markets. It connects to exchange WebSocket feeds (Binance SBE combined streams), maintains per-symbol limit order books, generates trading signals via pluggable strategies, and routes trade intents through risk checks before submitting via FIX protocol.

## Build

```bash
# One-time bootstrap
./vcpkg/bootstrap-vcpkg.sh
./vcpkg/vcpkg install openssl curl simdjson libpq

# Configure
cmake -B build -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j$(nproc)

# Run
cd build && ./MainExec
```

Requires: `.env` file with `BINANCE_API_KEY` and `BINANCE_SECRET_KEY`.
Stream subscriptions are driven by `exchanges_data.csv` (exchange, symbol, stream columns).

## Architecture: Data Pipeline

```
exchanges_data.csv
      ↓
StreamConfig → VenueRegistry (instrument_id mapping)
      ↓
MarketDataIngester  (one thread per venue, SBE WebSocket parser)
      ↓  LFQueue<MarketUpdate>
OrderBookManager    (routes by instrument_id → OrderBook[])
      ↓
OrderBook × N       (circular buffer LOB, 256 levels, O(1) updates)
      ↓
StrategyManager     (per-symbol dispatch)
      ↓  LFQueue<double>  (signals in [-1, 1])
PCModel → RiskModel → OrderGateway (FIX)
```

## Threading Model

- **Ingester thread(s)**: one per venue — TLS recv → SBE parse → enqueue MarketUpdate
- **OBM thread**: dequeue MarketUpdate → update OrderBook → dispatch to StrategyManager
- **Strategy/PCM thread**: consume signals → TradeIntent → risk check (not yet wired)
- **Gateway thread**: FIX order submission (not yet wired)

All inter-thread communication via `LFQueue<T>` (lock-free SPSC ring buffer). No mutexes on the hot path.

## Key Invariants

- `OrderBook` references returned by `OrderBookManager::book_for()` are **stable**: the vector is reserved at startup and never resized. References are safe to hold long-term.
- `register_instrument()` and `book_for()` must only be called **before** threads launch. They are not thread-safe.
- `VenueRegistry` is read-only after construction. Safe to share across threads.
- `instrument_id` is a packed `uint32_t`: bits[31:16] = venue index, bits[15:0] = symbol index. Assigned sequentially from CSV row order.

## Module Map

| Directory | Library target | Purpose |
|-----------|---------------|---------|
| `include/market_data/` | — | Market data types and pipeline headers |
| `include/strategies/` | — | Strategy interface and manager |
| `include/utils/` | — | Lock-free queue, memory pool, TLS, WebSocket, StreamConfig, LatencyTracker |
| `src/market_data/order_book/` | `OrderBook` | OrderBook + OrderBookManager |
| `src/market_data/data_ingester/` | `MarketDataIngester` | WebSocket SBE parser |
| `src/strategies/` | `MidPriceReversion`, `OrderBookImbalance`, `MicroMomentum` | Signal generators |
| `src/pcm_model/` | `PCModel` | Signal → TradeIntent sizing |
| `src/order_gateway/` | `OrderGateway` | FIX protocol (Ed25519, stunnel) |
| `src/utils/` | `TLSClient`, `WebSocket`, `StreamConfig`, `HttpClient` | Infrastructure |

See `include/market_data/CLAUDE.md`, `include/strategies/CLAUDE.md`, `include/utils/CLAUDE.md` for module-level detail.
