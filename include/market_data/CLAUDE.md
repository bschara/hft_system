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
struct alignas(32) MarketUpdate {
    uint32_t _instrument_id;
    Side     _side;           // BUY or SELL
    double   _price;
    double   _quantity;
    int64_t  _timestamp;      // exchange event time (microseconds)
};
```

A zero quantity means the level was removed from the book.

## OrderBook (`order_book/order_book.h`)

Circular buffer LOB: `std::array<PriceLevel, 256>` per side. Index arithmetic is modular — O(1) insert/update/delete. Key methods:
- `addUpdate(const MarketUpdate&)` — apply a depth update; `_quantity == 0` removes the level
- `getBestBidPrice()` / `getBestAskPrice()` — best quoted prices
- `getBestBidQuantity()` / `getBestAskQuantity()`
- `getNumOfBids()` / `getNumOfAsks()` — active level counts
- `priceToIndex(double price, bool is_bid)` — `(bestIndex + round((price - bestPrice) / TICK_SIZE) + LOB_DEPTH) % LOB_DEPTH`; higher index = higher price for **both** sides

**Indexing invariant:** `bestBidIndex` → highest bid price, walk `-1` for worse bids. `bestAskIndex` → lowest ask price, walk `+1` for worse asks. Updates more than `MAX_SHIFT_STEP` (64) ticks outside the current window are silently dropped.

The OrderBook does not know its own symbol; identity is managed externally by `OrderBookManager`.

## OrderBookManager (`order_book/order_book_manager.h`)

Owns a `std::vector<OrderBook>` pre-reserved at construction. Startup sequence (must happen before any threads launch):
```cpp
OrderBookManager obm(update_queue, registry.size());
for (auto& r : rows)
    obm.register_instrument(registry.lookup(r.exchange, r.symbol));
OrderBook& btc_book = obm.book_for(btc_id);  // stable reference — safe to hold
```

Hot path (`run()` thread): reads `LFQueue<MarketUpdate>`, calls `passUpdateToOrderbook()` which routes by `_instrument_id` and then calls `StrategyManager::onMarketData()`.

## MarketDataIngester (`data_ingester/market_data_ingester.h`)

One instance per venue. Owns references to a `TLSClient` and `WebSocket` (single connection — Binance combined streams multiplex all symbols). Constructor:
```cpp
MarketDataIngester ingester(update_queue, tls, ws, registry, "BINANCE");
ingester.startReceiving("/stream?streams=btcusdt@depth/ethusdt@depth", "");
```

`parseAndEnqueueUpdates()` pre-scans the SBE payload to read the trailing symbol field first, looks up `instrument_id` via `registry.lookup()`, then loops through bids and asks stamping every `MarketUpdate` with that id. This is the only call site for `registry.lookup()` at runtime — once per payload, not once per price level.

## Adding a New Symbol

1. Add a row to `exchanges_data.csv`
2. No code changes needed — `VenueRegistry` and `OrderBookManager` pick it up at startup
3. Optionally register a per-symbol strategy in `main.cpp` via `strategy_manager.register_strategy(id, &strat)`

## Adding a New Venue

1. Add rows to `exchanges_data.csv` with the new exchange name
2. In `main.cpp`, construct a new `TLSClient`, `WebSocket`, and `MarketDataIngester` for that venue
3. Launch a new ingester thread writing to the same `update_queue`
4. If the venue uses a different wire format than Binance SBE, subclass `MarketDataIngester` or add template-ID dispatch inside `parseAndEnqueueUpdates()`
