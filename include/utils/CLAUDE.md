# Utils Module

## LFQueue (`lock_free_queue.hpp`)

Lock-free SPSC (single-producer, single-consumer) ring buffer. Template over element type and capacity is set at construction (heap-allocated once).

```cpp
Common::LFQueue<MarketUpdate> q(4096);

// Producer
auto* slot = q.getNextToWriteTo();
*slot = MarketUpdate(...);
q.updateWriteIndex();

// Consumer
const MarketUpdate* item = q.getNextToRead();
if (item) {
    process(*item);
    q.updateReadIndex();
}
```

**Rules:**
- Exactly one producer thread and one consumer thread. No exceptions.
- Never call `getNextToWriteTo()` if the queue is full (check `size()` or design for sufficient capacity).
- Capacity must be a power of 2 for the modular arithmetic to work.
- The queue is not resizable after construction.

## MemPool (`memory_pool.hpp`)

Pre-allocated object pool — zero heap allocation after construction. Use for hot-path objects where `new`/`delete` latency is unacceptable. Not currently used in the main pipeline but available.

## TLSClient (`tls_client/tls_client.h`)

OpenSSL wrapper for a single TLS 1.2/1.3 TCP connection with SNI. **One instance = one connection**. For multiple venues, construct one `TLSClient` per venue.

```cpp
utility::TLSClient tls("stream.binance.com", 9443);
tls.connect();
tls.send(data, len);
tls.recv(buf, len);
```

## WebSocket (`websocket/websocket.h`)

RFC 6455 WebSocket client over a `TLSClient`. Handles handshake, ping/pong, and frame masking. **One instance = one connection.**

```cpp
utility::WebSocket ws(tls, "stream.binance.com", api_key);
ws.perform_handshake("/stream?streams=btcusdt@depth", "");
auto frame = ws.read_frame();  // returns std::optional<WebSocketFrame>
```

For Binance combined streams: all subscriptions are encoded in the URL path — no subscribe JSON frame is needed after handshake.

## StreamConfig (`stream_config/stream_config.h`)

Parses `exchanges_data.csv` (exchange, symbol, stream columns). Provides:
- `getRows()` — all `StreamRow` structs
- `buildBinanceURL()` — concatenates all BINANCE stream names into a combined WS URL

Adding a new symbol: add a CSV row. No code change required.

## EnvLoader (`env_loader.hpp`)

`loadEnv(path)` reads a `.env` file and sets environment variables. Call once at startup before any credential-dependent code.

## Benchmark (`benchmark/benchmark_utility.hpp`)

`rdtsc_start()` / `rdtsc_end()` — CPU cycle counters for nanosecond-level timing. Uses `CPUID` fences to prevent out-of-order execution from skewing results.

## LatencyTracker (`latency_tracker.hpp`)

`LatencyHistogram` — 64-bucket log₂ histogram for per-stage latency; O(1) `record()`, no allocations, single-threaded.
`calibrate_tsc_ns()` — 10 ms wall-clock spin to compute nanoseconds per TSC cycle. Call once at startup.
