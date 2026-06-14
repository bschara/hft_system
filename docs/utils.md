# Utilities

## LFQueue — Lock-Free Queue

**Header:** `include/utils/containers/lock_free_queue.hpp`

Single-producer / single-consumer ring buffer. No mutexes; uses `std::atomic` indices.

```cpp
Common::LFQueue<MarketUpdate> queue(1024);  // capacity must be power of 2

// Producer
MarketUpdate* slot = queue.getNextToWriteTo();
*slot = update;
queue.updateWriteIndex();

// Consumer
MarketUpdate* item = queue.getNextToRead();
if (item) {
    process(*item);
    queue.updateReadIndex();
}
```

- `size()` returns the current number of items
- Capacity is fixed at construction; no reallocation
- **Do not use from more than one producer or more than one consumer thread simultaneously**

---

## MemPool — Memory Pool

**Header:** `include/utils/containers/memory_pool.hpp`

Pre-allocated pool of `N` objects. Avoids heap allocation on the hot path by reusing freed slots.

```cpp
Common::MemPool<Order> pool(512);

Order* o = pool.allocate();   // returns a slot, constructs in place
// ... use o ...
pool.deallocate(o);           // marks slot free for reuse
```

- `allocate()` scans for a free slot starting from the last freed index
- Throws (or asserts) if pool is exhausted
- Not thread-safe — intended for use within a single thread

---

## TLSClient

**Header:** `include/utils/net/tls_client/tls_client.h`
**Source:** `src/utils/net/tls_client/tls_client.cpp`

OpenSSL wrapper for TLS 1.2/1.3 TCP connections.

```cpp
TLSClient tls;
tls.connect("stream.binance.com", "9443");
tls.send(data, len);
int n = tls.recv(buf, sizeof(buf));
int fd = tls.fd();   // raw socket FD, useful for select/epoll
```

- Performs DNS resolution and TCP connect before TLS handshake
- Sets SNI via `SSL_set_tlsext_host_name()` (required by Binance)
- Destructor cleans up `SSL*`, `SSL_CTX*`, and closes the socket

---

## WebSocket

**Header:** `include/utils/net/websocket/websocket.h`
**Source:** `src/utils/net/websocket/websocket.cpp`

RFC 6455 WebSocket client built on top of `TLSClient`.

```cpp
WebSocket ws(tls_client);
ws.perform_handshake("/stream?streams=btcusdt@depth");

ws.send_frame(payload, len);           // send a text/binary frame
WebSocketFrame frame = ws.read_frame(); // blocking read
// frame.fin, frame.opcode, frame.payload (std::span<uint8_t>)

uint64_t tsc = ws.io_done_tsc();       // TSC stamp from after last recv_exact()
```

- Handles **ping/pong** automatically — a ping frame received during `read_frame()` is replied to immediately
- Generates a random 4-byte mask for each sent frame (required by the RFC)
- Supports 7-bit, 16-bit, and 64-bit payload length encoding
- `WebSocketFrame.payload` is a span into an internal buffer; copy before the next `read_frame()` call
- `io_done_tsc()` returns the TSC timestamp written immediately after all `recv_exact()` calls complete, before mask XOR and opcode check — used by `MarketDataIngester` to split `ws_io` (network wait) from `ws_cpu` (decode work)

---

## HttpClient

**Header:** `include/utils/net/http_client/http_client.h`
**Source:** `src/utils/net/http_client/http_client.cpp`

Minimal libcurl wrapper for HTTP GET requests.

```cpp
HttpClient http;
std::string response = http.get("https://api.binance.com/api/v3/klines?symbol=BTCUSDT&interval=1m");
```

- SSL verification is enabled by default
- Response body is returned as `std::string`
- One instance per logical HTTP session (not thread-safe)

---

## Benchmark Utility

**Header:** `include/utils/perf/benchmark/benchmark_utility.hpp`

x86 CPU cycle counter for latency measurement.

```cpp
uint64_t start = rdtsc_start();  // CPUID + RDTSC (serialize before)
// ... hot path ...
uint64_t end = rdtsc_end();      // RDTSC + CPUID (serialize after)
uint64_t cycles = end - start;
```

Uses `CPUID` as a serializing instruction to prevent CPU out-of-order execution from skewing measurements. Suitable for microbenchmarks; for wall-clock latency use `CLOCK_MONOTONIC`.

---

## LatencyTracker

**Header:** `include/utils/perf/latency_tracker.hpp` (header-only)

Allocation-free per-stage latency histograms built on `rdtsc`. Used by `OrderBookManager` to measure queue wait, book update, strategy dispatch, and total OBM latency.

### LatencyHistogram

```cpp
Common::LatencyHistogram h("my_stage");

h.record(end_tsc - start_tsc);   // O(1) — single clz + bucket increment
h.report(ns_per_cycle);          // prints p50/p99/p999 to stdout
h.reset();                       // clear all buckets
```

Internally a 64-bucket log₂ histogram: bucket `b` holds all samples in `[2^b, 2^(b+1) - 1]` cycles. Covers 1 cycle to ~9×10¹⁸ cycles with constant-time recording. The struct is allocation-free and has no atomics — intended for use within a single thread.

### TSC Calibration

```cpp
double ns_per_cycle = Common::calibrate_tsc_ns();
```

Performs a ~10 ms busy-wait, compares the rdtsc delta to the `CLOCK_MONOTONIC` delta, and returns nanoseconds per TSC cycle. Call once at startup before launching threads; pass the result to `OrderBookManager::set_ns_per_cycle()`.

---

## Macros

**Header:** `include/utils/core/macros.h`

```cpp
LIKELY(x)    // __builtin_expect((x), 1) — branch prediction hint
UNLIKELY(x)  // __builtin_expect((x), 0)

ASSERT(cond) // Prints file:line and exits if cond is false (debug builds)
FATAL(msg)   // Prints msg and calls std::exit(EXIT_FAILURE)
```

Use `LIKELY`/`UNLIKELY` on hot-path conditionals where one branch dominates. Use `ASSERT` for invariants that must never be violated in production.

---

## EnvLoader

**Header:** `include/utils/config/env_loader.hpp`

Reads a `.env` file and sets each `KEY=VALUE` pair as an environment variable via `setenv()`.

```cpp
loadEnv(".env");
const char* key = std::getenv("BINANCE_API_KEY");
```

Call once at startup before any component that reads credentials. Multi-line values (e.g., PEM private keys) require the value to be on a single line or use `\n` escapes.

---

## CSV Reader

**Header:** `include/utils/config/csv.h`

Header-only CSV parser (Ben Strasser, MIT license). Template parameter is column count.

```cpp
io::CSVReader<4> csv("exchanges_data.csv");
csv.read_header(io::ignore_extra_column, "exchange", "symbol", "base", "stream");
std::string exchange, symbol, base, stream;
while (csv.read_row(exchange, symbol, base, stream)) {
    // process row
}
```

Used internally by `StreamConfig`. Handles quoted fields, custom delimiters, and missing columns.
