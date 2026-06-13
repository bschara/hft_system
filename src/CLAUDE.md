# Source / Build Conventions

## CMake Library Targets

Each subdirectory under `src/` is a separately-compiled static library with its own `CMakeLists.txt`. The root `CMakeLists.txt` adds all subdirectories via `add_subdirectory()` and links selected targets into `MainExec`.

| Target | Sources | Notes |
|--------|---------|-------|
| `OrderBook` | `order_book.cpp`, `order_book_manager.cpp` | Both in same library; OBM uses OrderBook types directly |
| `MarketDataIngester` | `market_data_ingester.cpp` | Links against `OrderBook`, `WebSocket`, `StreamConfig` transitively |
| `MidPriceReversion` | `mean_reversion/midprice_reversion.cpp` | |
| `OrderBookImbalance` | `mean_reversion/orderbook_imbalance.cpp` | |
| `MicroMomentum` | `trend_following/micro_momentum.cpp` | |
| `TLSClient` | `tls_client.cpp` | Links OpenSSL |
| `WebSocket` | `websocket.cpp` | Links TLSClient |
| `StreamConfig` | `stream_config.cpp` | Links csv.h (header-only) |
| `HttpClient` | `http_client.cpp` | Links libcurl |
| `PCModel` | `pcm_model.cpp` | |
| `OrderGateway` | `order_gateway.cpp` | Links QuickFIX (submodule at `/workspace/quickfix`) |
| `HistoricalDataAggergator` | `historical_aggregator.cpp` | Links libpq (PostgreSQL) |
| `OrderManagementSystem` | `order_management_system.cpp` | Stub |

## Adding a New Submodule

1. Create `include/<module>/` and `src/<module>/`
2. Write `src/<module>/CMakeLists.txt`:
   ```cmake
   add_library(MyModule my_module.cpp)
   target_include_directories(MyModule PUBLIC ${PROJECT_SOURCE_DIR}/include)
   target_link_libraries(MyModule PUBLIC <dependencies>)
   ```
3. Add `src/<module>` to `SUBMODULE_DIRS` in root `CMakeLists.txt`
4. Add `MyModule` to `target_link_libraries(MainExec ...)` in root `CMakeLists.txt`

## Header-Only Utilities

Files in `include/utils/` that are header-only (no `.cpp`):
- `lock_free_queue.hpp`
- `memory_pool.hpp`
- `env_loader.hpp`
- `benchmark/benchmark_utility.hpp`
- `csv.h`
- `macros.h`
- `types.h`

And in `include/market_data/`:
- `venue_registry.hpp`
- `strategies/strategy_manager.hpp`

These need no CMake library target and are included directly.

## Entry Point

`src/main.cpp` wires all components together:
1. Load env, parse CSV, build `VenueRegistry`; calibrate TSC (`calibrate_tsc_ns()`)
2. Construct one `LFQueue<MarketUpdate>` per venue; construct `LFQueue<double>` for signals (capacity 8192)
3. Construct `OrderBookManager`, register instruments, call `add_queue()` for each venue queue
4. Construct `StrategyManager`; register all three strategy types (`MidPriceReversion`, `OrderBookImbalance`, `MicroMomentum`) per symbol
5. Construct per-venue `TLSClient` + `WebSocket` + `MarketDataIngester`
6. Launch threads (OBM thread, one ingester thread per venue)
