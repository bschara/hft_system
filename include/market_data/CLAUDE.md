# Market Data Module

## Instrument Identity

Every market event is tagged with a `uint32_t instrument_id` (defined in `market_update.h`):
```
bits[31:16] = venue_idx   (0 = BINANCE, 1 = KRAKEN, ...)
bits[15:0]  = symbol_idx  (0 = BTCUSDT, 1 = ETHUSDT, ...)
```

`VenueRegistry` (`venue_registry.hpp`) is the single source of truth. It is built at startup from `StreamConfig::getRows()` and is read-only thereafter. Use `registry.lookup("BINANCE", "BTCUSDT")` to get an id; use `VenueRegistry::venue_of(id)` / `symbol_of(id)` to decode.

## MarketUpdate (`order_book/market_update.h`)

```cpp
struct alignas(64) MarketUpdate {
    uint32_t _instrument_id;
    Side     _side;           // BUY or SELL
    int64_t  _price;          // raw integer from exchange
    int64_t  _quantity;       // raw integer (0 = remove level)
    int8_t   _price_exp;      // actual price = _price × 10^_price_exp
    int8_t   _qty_exp;        // actual qty   = _quantity × 10^_qty_exp
    int64_t  _timestamp;      // exchange event time (microseconds)
    uint64_t _recv_tsc;       // rdtsc at ingester enqueue; used for queue-wait latency
};
```

Prices and quantities are stored as raw integers — **no float conversion on the hot path**. Convert to double only for display or PCModel output via `PriceUtils::to_double(raw, exp)`. A zero quantity means the level was removed from the book.

## OrderBook (`order_book/order_book.h`)

Circular buffer LOB: `std::array<PriceLevel, 256>` per side. `PriceLevel` stores raw `int64_t` price and quantity. Index arithmetic is modular — O(1) insert/update/delete. Key methods:
- `addUpdate(const MarketUpdate&)` — apply a depth update; `_quantity == 0` removes the level
- `getBestBidRaw()` / `getBestAskRaw()` — best prices as raw integers
- `getBestBidQtyRaw()` / `getBestAskQtyRaw()` — best quantities as raw integers
- `getMidRaw()` — volume-weighted mid price in raw integer units (uses `__int128` sums)
- `getMidPrice()` — double mid price, for PCModel / display only (`= PriceUtils::to_double(getMidRaw(), price_exp_)`)
- `getNumOfBids()` / `getNumOfAsks()` — active level counts
- `priceToIndex(int64_t price, bool is_bid)` — `(bestIndex + (price - bestPrice) / TICK_UNITS + LOB_DEPTH) % LOB_DEPTH`; higher index = higher price for **both** sides

**Indexing invariant:** `bestBidIndex` → highest bid price, walk `-1` for worse bids. `bestAskIndex` → lowest ask price, walk `+1` for worse asks. Updates more than `MAX_SHIFT_STEP` (64) raw ticks outside the current window are silently dropped.

The OrderBook does not know its own symbol; identity is managed externally by `OrderBookManager`.

## OrderBookManager (`order_book/order_book_manager.h`)

Owns a `std::vector<OrderBook>` pre-reserved at construction and a `std::vector<LFQueue<MarketUpdate>*>` of per-venue queues. Startup sequence (must happen before any threads launch):
```cpp
OrderBookManager obm(registry.size());           // no queue in constructor
for (auto& r : rows)
    obm.register_instrument(registry.lookup(r.exchange, r.symbol));
obm.add_queue(binance_queue);                    // one call per venue
// obm.add_queue(kraken_queue);
OrderBook& btc_book = obm.book_for(btc_id);     // stable reference — safe to hold
```

Hot path (`run()` thread): reads `LFQueue<MarketUpdate>`, stamps `dequeue_tsc`, calls `passUpdateToOrderbook()` which routes by `_instrument_id`, wraps `addUpdate()` and `onMarketData()` with `rdtsc` fences for per-stage latency histograms, then calls `StrategyManager::onMarketData()`. Every 1,000,000 updates, `report_latencies()` prints p50/p99/p999 to stdout.

## Schema-Driven Ingestion (`data_ingester/`)

The ingestion pipeline is split into three layers:

**`VenueParser`** (`venue_parser.h`) — abstract interface:
```cpp
class VenueParser {
public:
    virtual void parse(std::span<const uint8_t> payload,
                       Common::LFQueue<MarketUpdate>& queue) = 0;
};
```

**`SBEVenueParser`** (`sbe_venue_parser.h`) — outer dispatch loop: reads `templateId` from SBE header, looks up `MessageSchema` in `SchemaRegistry`, delegates to `SBEDecoder::decode()`. Supports multiple SBE messages in one WebSocket frame.

**`SBEDecoder`** (`sbe_decoder.h`) — stateless decoder: reads raw integer prices and quantities directly from the SBE binary layout described by `MessageSchema`. No float arithmetic. Returns bytes consumed per message.

**`MessageSchema`** (`message_schema.h`) — pure data: field byte-offsets, encoding types, timestamp units, group structure metadata, and a `skip` flag for a single SBE templateId. Four Binance schemas are defined as `constexpr`: `kBinanceDepthDiff` (10003), `kBinanceDepthSnapshot` (10002), `kBinanceBestBidAsk` (10001, skip=true), `kBinanceTrades` (10000, skip=true). Skip schemas allow `SBEVenueParser` to advance past messages with incompatible group structures via `SBEDecoder::measure()`.

**`MarketDataIngester`** (`market_data_ingester.h`) — thin shell: TLS + WebSocket receive loop; delegates every binary frame to `VenueParser::parse()`.

### Constructor / startup:
```cpp
SchemaRegistry binance_schemas;
binance_schemas.registerSchema(kBinanceDepthDiff);
binance_schemas.registerSchema(kBinanceDepthSnapshot);
binance_schemas.registerSchema(kBinanceBestBidAsk);  // skip=true
binance_schemas.registerSchema(kBinanceTrades);       // skip=true

SBEVenueParser binance_parser(std::move(binance_schemas), registry, "BINANCE");
MarketDataIngester ingester(binance_queue, tls, ws, binance_parser);
ingester.startReceiving("/stream?streams=btcusdt@depth/ethusdt@depth", "");
```

## Price Utilities (`utils/pricing/price_utils.hpp`)

For logging, display, and PCModel output only — never on the hot path:
```cpp
PriceUtils::to_double(int64_t raw, int8_t exp)  // raw × 10^exp
PriceUtils::to_string(int64_t raw, int8_t exp)  // formatted string
```

## Adding a New Symbol

1. Add a row to `exchanges_data.csv`
2. No code changes needed — `VenueRegistry` and `OrderBookManager` pick it up at startup
3. Optionally register a per-symbol strategy in `main.cpp` via `strategy_manager.register_strategy(id, &strat)`

## Adding a New Venue

1. Add rows to `exchanges_data.csv` with the new exchange name
2. In `main.cpp`, construct a dedicated `LFQueue<MarketUpdate>` for that venue
3. Build a `SchemaRegistry` with the venue's schemas, then a `SBEVenueParser` (or subclass `VenueParser` for a non-SBE format)
4. Construct a `MarketDataIngester` pointing at the new parser
5. Call `obm.add_queue(new_venue_queue)` before launching threads
6. Launch a new ingester thread — it writes exclusively to its own queue (SPSC preserved)
