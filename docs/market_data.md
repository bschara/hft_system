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

## MarketDataIngester

**Header:** `include/market_data/data_ingester/market_data_ingester.h`
**Source:** `src/market_data/data_ingester/market_data_ingester.cpp`

One instance per venue. Owns the full WebSocket lifecycle for that venue's combined stream:

1. Calls `TLSClient::connect()` to open a TLS socket
2. Calls `WebSocket::perform_handshake(path)` to upgrade; path encodes all subscriptions
3. Enters a `read_frame()` loop; each binary frame is passed to `parseAndEnqueueUpdates()`

Constructor:
```cpp
MarketDataIngester ingester(update_queue, tls_client, web_socket, registry, "BINANCE");
ingester.startReceiving("/stream?streams=btcusdt@depth/ethusdt@depth/bnbusdt@depth", "");
```

For Binance combined streams, no subscription JSON frame is sent after the handshake — subscriptions are encoded in the URL path.

### SBE Parsing

Binance depth streams use **Simple Binary Encoding (SBE)**. The parser in `parseAndEnqueueUpdates()` reads:

```
[SBE header: 8 bytes]
  uint16 blockLength, templateId, schemaId, version

[Fixed block: 26 bytes]
  int64  eventTime
  int64  firstBookUpdateID
  int64  lastBookUpdateID
  int8   priceExponent        ← scale: actual_price = raw × 10^exp
  int8   qtyExponent

[Bid group]
  uint16 bidBlockLength
  uint16 numBids
  for each bid:
    int64 priceRaw
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

**Symbol extraction:** The parser pre-scans the payload to locate the trailing symbol field before entering the bid/ask loops. It calls `registry.lookup(venue_name, symbol)` once per payload to get `instrument_id`, then stamps every `MarketUpdate` in that payload with it.

Each price level becomes a `MarketUpdate` enqueued to the shared `LFQueue<MarketUpdate>`.

### Key Types

```cpp
struct alignas(32) MarketUpdate {
    uint32_t _instrument_id;   // packed venue+symbol index
    Side     _side;            // BUY or SELL
    double   _price;           // actual_price = raw × 10^priceExp
    double   _quantity;        // actual_qty  = raw × 10^qtyExp  (0 = level removed)
    int64_t  _timestamp;       // exchange event time
};
```

---

## OrderBook

**Header:** `include/market_data/order_book/order_book.h`
**Source:** `src/market_data/order_book/order_book.cpp`

Maintains a live Limit Order Book for a single symbol using a **circular buffer** of 256 price levels per side. The `OrderBook` does not know its own symbol — identity is managed by `OrderBookManager`.

### Constants

| Constant | Value | Meaning |
|----------|-------|---------|
| `LOB_DEPTH` | 256 | Price levels stored per side |
| `TICK_SIZE` | 0.01 | Minimum price increment |
| `QUANTITY_STEP_SIZE` | 0.0001 | Minimum quantity increment |
| `MID_PRICE_N` | 5 | Levels used for volume-weighted mid-price |

### Indexing

Price → buffer slot relative to the current best price:

```
index = (bestIndex + round((price - bestPrice) / TICK_SIZE) + LOB_DEPTH) % LOB_DEPTH
```

This convention is the same for both sides — a price one tick *above* best maps to `bestIndex + 1`. For bids, `bestBidIndex` holds the highest price and `getMidPrice` walks in the `-1` direction toward lower (worse) prices. For asks, `bestAskIndex` holds the lowest price and `getMidPrice` walks in the `+1` direction toward higher (worse) prices. Updates beyond `MAX_SHIFT_STEP = 64` ticks from the current window are silently dropped to prevent index overflow.

### Zero-Quantity Removal

A `MarketUpdate` with `_quantity == 0` removes the level at that price. `addUpdate()` clears `_isActive`, decrements the count, and if the removed level was the best or worst, scans for the next active level to update the endpoint index.

### Key Methods

| Method | Description |
|--------|-------------|
| `addUpdate(const MarketUpdate&)` | Apply a depth update; qty=0 removes the level |
| `getMidPrice()` | Volume-weighted average of up to `MID_PRICE_N` bid and ask levels |
| `getBestBidPrice()` / `getBestAskPrice()` | Best quoted prices |
| `getBestBidQuantity()` / `getBestAskQuantity()` | Quantities at best prices |
| `getNumOfBids()` / `getNumOfAsks()` | Active level counts |

### PriceLevel Layout

```cpp
struct alignas(32) PriceLevel {
    double _price        = 0;
    double _totalQuantity = 0;
    bool   _isActive     = false;
};
```

---

## OrderBookManager

**Header:** `include/market_data/order_book/order_book_manager.h`
**Source:** `src/market_data/order_book/order_book_manager.cpp`

Routes `MarketUpdate` objects from the shared queue to the correct `OrderBook` instance. Stores `OrderBook` objects in a flat `std::vector` pre-reserved at construction — no heap allocation after startup, and references are stable.

### Startup Sequence (must complete before any threads launch)

```cpp
OrderBookManager obm(update_queue, registry.size());

// Register every instrument from the CSV
for (const auto& r : rows)
    obm.register_instrument(registry.lookup(r.exchange, r.symbol));

// Bind strategies to their books (references are stable after reserve)
OrderBook& btc_book = obm.book_for(btc_id);

// Optionally wire the strategy manager
obm.set_strategy_manager(&strategy_manager);
```

### Hot Path

`run()` dequeues one `MarketUpdate` at a time, looks up the `instrument_id` in an `unordered_map<uint32_t, uint16_t>` to get an array index, calls `OrderBook::addUpdate()`, then calls `StrategyManager::onMarketData()`.

### API

| Method | Thread | Description |
|--------|--------|-------------|
| `register_instrument(uint32_t id)` | startup only | Allocates one OrderBook slot |
| `book_for(uint32_t id)` | startup only | Returns stable `OrderBook&` |
| `set_strategy_manager(StrategyManager*)` | startup only | Wires signal dispatch |
| `run()` | OBM thread | Consumer loop |
| `stop()` | any | Sets flag to exit `run()` |

---

## HistoricalDataAggregator

**Header:** `include/market_data/historical_data_aggregator/historical_aggregator.h`
**Source:** `src/market_data/historical_data_aggregator/historical_aggregator.cpp`

Fetches and stores historical tick data from the Binance REST API.

- Connects to PostgreSQL via `PQconnectdb()` on construction
- Creates `tick_data` table if it does not exist:
  ```sql
  CREATE TABLE tick_data (
      time        TIMESTAMPTZ,
      side        TEXT,
      price       DOUBLE PRECISION,
      quantity    DOUBLE PRECISION
  );
  ```
- `fetchHistoricalData()` — HTTP GET to Binance klines endpoint, parses JSON with simdjson
- `ComputeVolatility()` — Returns annualized volatility from stored returns
- `insert_tick_data()` / `writeToDB()` — Batch inserts to PostgreSQL

Status: partially implemented. PostgreSQL connection and schema creation work; data fetch and compute methods are in progress.

---

## Stream Configuration

**Header:** `include/utils/stream_config/stream_config.h`
**Source:** `src/utils/stream_config/stream_config.cpp`

Loads `exchanges_data.csv` and builds the Binance combined stream WebSocket URL.

```cpp
StreamConfig config("exchanges_data.csv");
const auto& rows = config.getRows();   // all StreamRow structs
std::string url  = config.buildBinanceURL();
// → "wss://stream.binance.com:9443/stream?streams=btcusdt@depth/ethusdt@depth/bnbusdt@depth"
```

`StreamRow` fields: `exchange`, `symbol`, `stream`. The `stream` column value maps directly to the Binance stream name token.
