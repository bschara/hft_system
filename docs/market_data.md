# Market Data

## MarketDataIngester

**Header:** `include/market_data/data_ingester/market_data_ingester.h`
**Source:** `src/market_data/data_ingester/market_data_ingester.cpp`

Owns the full WebSocket lifecycle for a single exchange stream:

1. Calls `TLSClient::connect(host, port)` to open a TLS socket
2. Calls `WebSocket::perform_handshake(path)` to upgrade to WebSocket
3. Sends a JSON subscription message for the configured streams
4. Enters a `read_frame()` loop; each frame is passed to `parseAndEnqueueUpdates()`

### SBE Parsing

Binance depth streams use **Simple Binary Encoding (SBE)**, not JSON. The parser in `parseAndEnqueueUpdates()` reads:

```
[4-byte block header]  blockLength, templateId, schemaId, version
[price exponent]       int8 — scale factor for all prices  (price × 10^exp)
[qty exponent]         int8 — scale factor for all quantities
[numBids]              uint8
  for each bid:        [int64 price] [int64 qty]
[numAsks]              uint8
  for each ask:        [int64 price] [int64 qty]
```

Scaled prices: `actual_price = raw_int64 × pow(10, price_exp)`

Each level becomes a `MarketUpdate` enqueued to `LFQueue<MarketUpdate>`.

**Known bug:** The ask-loop currently iterates `numBids` times instead of `numAsks`. See `src/market_data/data_ingester/market_data_ingester.cpp:154`.

### Key Types

```cpp
struct MarketUpdate {
    int32_t  symbol_id;
    Side     side;         // BUY or SELL
    double   price;
    double   quantity;
    int64_t  timestamp;
};
```

---

## OrderBook

**Header:** `include/market_data/order_book/order_book.h`
**Source:** `src/market_data/order_book/order_book.cpp`

Maintains a live Limit Order Book for a single symbol using a **circular buffer** of 256 price levels per side.

### Constants

| Constant | Value | Meaning |
|---|---|---|
| `LOB_DEPTH` | 256 | Number of price levels stored per side |
| `TICK_SIZE` | 0.01 | Minimum price increment |
| `QUANTITY_STEP_SIZE` | 0.0001 | Minimum quantity increment |
| `MID_PRICE_N` | 5 | Levels used for volume-weighted mid-price |

### Indexing

```cpp
int priceToIndex(double price) const {
    return static_cast<int>(price / TICK_SIZE) % LOB_DEPTH;
}
```

When prices move enough that levels fall outside the active window, `shiftBookToPrice()` clears stale slots and resets the base reference.

### Key Methods

| Method | Description |
|---|---|
| `addUpdate(MarketUpdate&)` | Insert or update a price level on the correct side |
| `getMidPrice()` | Volume-weighted average of best N bid and ask levels |
| `getBestBidPrice()` | Highest bid price |
| `getBestAskPrice()` | Lowest ask price |
| `getBestBidQuantity()` | Quantity at best bid |
| `getBestAskQuantity()` | Quantity at best ask |

### PriceLevel Layout

```cpp
struct alignas(32) PriceLevel {
    double price;
    double quantity;
};
```

32-byte alignment keeps a pair of levels in one cache line.

---

## OrderBookManager

**Header:** `include/market_data/order_book/order_book_manager.h`
**Source:** `src/market_data/order_book/order_book_manager.cpp`

Routes `MarketUpdate` objects from the queue to the correct `OrderBook` instance.

- Maintains a `symbol_to_index` map (string symbol → array index)
- Currently hard-coded for 3 symbols (matches `exchanges_data.csv`)
- `run()` is the consumer loop: dequeue → `passUpdateToOrderbook()` → repeat
- `stop()` sets an atomic flag to exit the loop cleanly

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
// exchanges_data.csv format:
// exchange,symbol,exchange_base_connection,stream
// BINANCE,BTCUSDT,,btcusdt@depth

StreamConfig config("exchanges_data.csv");
std::string url = config.buildBinanceURL();
// → "wss://stream.binance.com:9443/stream?streams=btcusdt@depth/ethusdt@depth/bnbusdt@depth"
```
