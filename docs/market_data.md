# Market Data

## VenueRegistry

**Header:** `include/market_data/venue_registry.hpp` (header-only)

The single source of truth mapping `(exchange, symbol)` pairs to a packed `uint32_t instrument_id`.

```
instrument_id bits [31:16] = venue index   (0 = BINANCE, 1 = KRAKEN, ...)
instrument_id bits [15:0]  = symbol index  (0 = BTCUSDT, 1 = ETHUSDT, ...)
```

Constructed once at startup from `StreamConfig::getRows()`. Read-only after construction — safe to share across threads without synchronization.

```cpp
StreamConfig config("exchanges_data.csv");
VenueRegistry registry(config.getRows());

uint32_t id = registry.lookup("BINANCE", "BTCUSDT");  // → 0x00000000
uint32_t id = registry.lookup("BINANCE", "ETHUSDT");  // → 0x00000001

// Decode
uint16_t venue_idx  = VenueRegistry::venue_of(id);
uint16_t symbol_idx = VenueRegistry::symbol_of(id);
```

Adding a new symbol: add a row to `exchanges_data.csv`. No code changes needed.

---

## MarketUpdate

**Header:** `include/market_data/order_book/market_update.h`

```cpp
struct alignas(64) MarketUpdate {
    uint32_t _instrument_id;   // packed venue+symbol index
    Side     _side;            // BUY or SELL
    int64_t  _price;           // raw integer from exchange
    int64_t  _quantity;        // raw integer (0 = remove level)
    int8_t   _price_exp;       // actual price = _price × 10^_price_exp
    int8_t   _qty_exp;         // actual qty   = _quantity × 10^_qty_exp
    int64_t  _timestamp;       // exchange event time (microseconds)
    uint64_t _recv_tsc;        // rdtsc at ingester enqueue; used for queue-wait latency
};
```

Prices and quantities are stored as **raw integers** — no float conversion happens on the hot path. To get a displayable double value use `PriceUtils::to_double(raw, exp)` (never on hot path). A zero quantity signals level removal.

`_recv_tsc` is stamped once per SBE payload (before the bid/ask loops) and copied to every level in that payload. All levels parsed from the same WebSocket frame share the same ingestion timestamp.

---

## Schema-Driven Ingestion Pipeline

The ingestion layer is split into four components, from innermost to outermost:

### MessageSchema (`data_ingester/message_schema.h`)

A pure data struct describing the binary layout of one SBE templateId:

```cpp
struct MessageSchema {
    uint16_t      template_id;
    uint16_t      schema_id;
    size_t        fixed_block_size;
    size_t        event_time_offset;
    TimestampUnit event_time_unit;     // MILLISECONDS → normalised to microseconds
    size_t        price_exp_offset;    // within fixed block
    size_t        qty_exp_offset;
    ScaledField   price_field;         // { offset, encoding } within a level block
    ScaledField   qty_field;
    SideFieldDef  side_field;          // IMPLICIT_BY_GROUP or INLINE_FIELD
};
```

The Binance depth schema is a `constexpr` defined in the header (`kBinanceDepthV1`). Adding a new schema means filling in a new `MessageSchema` struct — no parser code changes.

### SchemaRegistry (`data_ingester/schema_registry.h`)

Maps `templateId → MessageSchema`. Header-only, constructed at startup:
```cpp
SchemaRegistry binance_schemas;
binance_schemas.registerSchema(kBinanceDepthV1);
```

### SBEDecoder (`data_ingester/sbe_decoder.h`)

Stateless pure decoder. The Binance SBE wire format is:

```
[SBE header: 8 bytes]
  uint16 blockLength, templateId, schemaId, version

[Fixed block: 26 bytes]
  int64  eventTime            ← normalised to microseconds
  int64  firstBookUpdateID
  int64  lastBookUpdateID
  int8   priceExponent        ← shared across all levels in this message
  int8   qtyExponent

[Bid group]
  uint16 bidBlockLength
  uint16 numBids
  for each bid:
    int64 priceRaw            ← stored as-is in MarketUpdate._price
    int64 qtyRaw

[Ask group]
  uint16 askBlockLength
  uint16 numAsks
  for each ask:
    int64 priceRaw
    int64 qtyRaw

[Trailing symbol field]
  uint8  symbolLength
  char[] symbolName           ← e.g. "BTCUSDT"
```

`SBEDecoder::decode()` pre-scans to the trailing symbol field, resolves `instrument_id` via `VenueRegistry`, then enqueues one `MarketUpdate` per level. Returns bytes consumed so the outer loop can advance the cursor. **No floating-point arithmetic.**

### SBEVenueParser (`data_ingester/sbe_venue_parser.h`)

Outer dispatch loop over a WebSocket frame:
1. Read `templateId` from SBE header at the current cursor
2. Look up `MessageSchema` in `SchemaRegistry`
3. Call `SBEDecoder::decode()` → advance cursor by bytes consumed
4. Repeat until the frame is exhausted or an unknown templateId is encountered

Implements `VenueParser` so `MarketDataIngester` is decoupled from any specific encoding.

### VenueParser (`data_ingester/venue_parser.h`)

Abstract interface:
```cpp
class VenueParser {
public:
    virtual void parse(std::span<const uint8_t> payload,
                       Common::LFQueue<MarketUpdate>& queue) = 0;
};
```

### MarketDataIngester (`data_ingester/market_data_ingester.h`)

Thin shell — WebSocket receive loop only:

```cpp
MarketDataIngester ingester(binance_queue, tls_client, web_socket, binance_parser);
ingester.startReceiving("/stream?streams=btcusdt@depth/ethusdt@depth", "");
```

On each binary frame, calls `parser_.parse(frame->payload, updatesQueue)`. No SBE awareness in the ingester itself.

### Full Startup Wiring

```cpp
SchemaRegistry binance_schemas;
binance_schemas.registerSchema(kBinanceDepthV1);

SBEVenueParser binance_parser(std::move(binance_schemas), registry, "BINANCE");

utility::TLSClient binance_tls("stream.binance.com", 9443);
utility::WebSocket binance_ws(binance_tls, "stream.binance.com", "");

MarketDataIngester binance_ingester(binance_queue, binance_tls, binance_ws, binance_parser);
```

---

## OrderBook

**Header:** `include/market_data/order_book/order_book.h`
**Source:** `src/market_data/order_book/order_book.cpp`

Maintains a live Limit Order Book for a single symbol using a **circular buffer** of 256 price levels per side. Prices and quantities are stored as raw integers matching the `MarketUpdate` fields. The `OrderBook` does not know its own symbol — identity is managed by `OrderBookManager`.

### Constants

| Constant | Value | Meaning |
|----------|-------|---------|
| `LOB_DEPTH` | 256 | Price levels stored per side |
| `TICK_UNITS` | 1 | Minimum raw integer tick (one unit = one price step) |
| `MID_PRICE_N` | 5 | Levels used for volume-weighted mid-price |
| `MAX_SHIFT_STEP` | 64 | Max raw ticks outside window before update is dropped |

### PriceLevel Layout

```cpp
struct alignas(32) PriceLevel {
    int64_t _price    = 0;
    int64_t _quantity = 0;
    bool    _isActive = false;
};
```

### Indexing

Price → buffer slot relative to the current best price:

```
index = (bestIndex + (price - bestPrice) / TICK_UNITS + LOB_DEPTH) % LOB_DEPTH
```

This convention is the same for both sides — a price one tick above best maps to `bestIndex + 1`. For bids, `bestBidIndex` holds the highest price and `getMidRaw` walks in the `-1` direction toward lower (worse) prices. For asks, `bestAskIndex` holds the lowest price and `getMidRaw` walks in the `+1` direction toward higher (worse) prices. Updates beyond `MAX_SHIFT_STEP = 64` ticks from the current window are silently dropped.

### Key Methods

| Method | Returns | Description |
|--------|---------|-------------|
| `addUpdate(const MarketUpdate&)` | void | Apply a depth update; qty=0 removes the level |
| `getMidRaw()` | `int64_t` | Volume-weighted mid price (raw units); uses `__int128` sums |
| `getMidPrice()` | `double` | `PriceUtils::to_double(getMidRaw(), price_exp_)` — for PCModel/display only |
| `getBestBidRaw()` | `int64_t` | Best bid price (raw integer) |
| `getBestAskRaw()` | `int64_t` | Best ask price (raw integer) |
| `getBestBidQtyRaw()` | `int64_t` | Best bid quantity (raw integer) |
| `getBestAskQtyRaw()` | `int64_t` | Best ask quantity (raw integer) |
| `getNumOfBids()` / `getNumOfAsks()` | `int` | Active level counts |
| `price_exp()` / `qty_exp()` | `int8_t` | Exponents latched from first update |

`getMidRaw()` uses `__int128` for the `price × quantity` accumulator sums to prevent overflow on large raw integers.

### Zero-Quantity Removal

A `MarketUpdate` with `_quantity == 0` removes the level at that price. `addUpdate()` clears `_isActive`, decrements the count, and if the removed level was the best or worst, scans for the next active level to update the endpoint index.

---

## OrderBookManager

**Header:** `include/market_data/order_book/order_book_manager.h`
**Source:** `src/market_data/order_book/order_book_manager.cpp`

Routes `MarketUpdate` objects from per-venue queues to the correct `OrderBook` instance. Stores `OrderBook` objects in a flat `std::vector` pre-reserved at construction — no heap allocation after startup, and references are stable.

### Startup Sequence (must complete before any threads launch)

```cpp
OrderBookManager obm(registry.size());   // no queue in constructor

// Register every instrument from the CSV
for (const auto& r : rows)
    obm.register_instrument(registry.lookup(r.exchange, r.symbol));

// Register one queue per venue (each must have exactly one producer thread)
obm.add_queue(binance_queue);
// obm.add_queue(kraken_queue);  // add more venues here, no other changes needed

// Bind strategies to their books (references are stable after reserve)
OrderBook& btc_book = obm.book_for(btc_id);

// Optionally wire the strategy manager
obm.set_strategy_manager(&strategy_manager);
```

### Hot Path

`run()` round-robins over all registered queues, stamps `dequeue_tsc = rdtsc_start()`, dequeues one `MarketUpdate`, looks up the `instrument_id` in an `unordered_map<uint32_t, uint16_t>` to get an array index, calls `OrderBook::addUpdate()`, then calls `StrategyManager::onMarketData()`.

### API

| Method | Thread | Description |
|--------|--------|-------------|
| `register_instrument(uint32_t id)` | startup only | Allocates one OrderBook slot |
| `book_for(uint32_t id)` | startup only | Returns stable `OrderBook&` |
| `add_queue(LFQueue<MarketUpdate>&)` | startup only | Registers a per-venue queue; one per ingester thread |
| `set_strategy_manager(StrategyManager*)` | startup only | Wires signal dispatch |
| `set_ns_per_cycle(double)` | startup only | Sets TSC→ns factor for latency reports |
| `run()` | OBM thread | Round-robins over all queues; records latency histograms |
| `report_latencies()` | OBM thread | Prints p50/p99/p999 for all stages; called every 1M updates |
| `stop()` | any | Sets atomic flag to exit `run()` |

---

## Price Utilities

**Header:** `include/utils/price_utils.hpp` (header-only)

For display, logging, and PCModel output only — **never called on the hot path**:

```cpp
// Convert raw integer + exponent to double
double PriceUtils::to_double(int64_t raw, int8_t exp);   // raw × 10^exp

// Format as a string (for logging)
std::string PriceUtils::to_string(int64_t raw, int8_t exp);
```

Example: `PriceUtils::to_double(4500000, -2)` → `45000.00` (BTCUSDT at $45,000).

---

## HistoricalDataAggregator

**Header:** `include/market_data/historical_data_aggregator/historical_aggregator.h`
**Source:** `src/market_data/historical_data_aggregator/historical_aggregator.cpp`

Fetches and stores historical tick data from the Binance REST API.

- Connects to PostgreSQL via `PQconnectdb()` on construction
- Creates `tick_data` table if it does not exist
- `fetchHistoricalData()` — HTTP GET to Binance klines endpoint, parses JSON with simdjson
- `ComputeVolatility()` — Returns annualized volatility from stored returns

Status: partially implemented.

---

## Stream Configuration

**Header:** `include/utils/stream_config/stream_config.h`

Loads `exchanges_data.csv` and builds the Binance combined stream WebSocket URL.

```cpp
StreamConfig config("exchanges_data.csv");
const auto& rows = config.getRows();   // all StreamRow structs
std::string url  = config.buildBinanceURL();
// → "wss://stream.binance.com:9443/stream?streams=btcusdt@depth/ethusdt@depth/bnbusdt@depth"
```
