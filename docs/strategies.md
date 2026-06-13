# Strategies

## Strategy Interface

**Header:** `include/strategies/strategy.h`

All strategies implement one virtual method:

```cpp
class Strategy {
public:
    virtual double onMarketData(const MarketUpdate*) = 0;
    virtual ~Strategy() = default;
};
```

Return value is a **signal** in `[-1.0, 1.0]`:
- `+1.0` — maximum buy conviction
- `-1.0` — maximum sell conviction
- `0.0` — no edge / flat

The `MarketUpdate` passed to `onMarketData` includes `_instrument_id`, so a strategy can inspect which instrument triggered the call.

---

## StrategyManager

**Header:** `include/strategies/strategy_manager.hpp` (header-only)

Dispatches market updates to strategies. Supports two registration modes:

```cpp
// Per-symbol: strategy only receives updates for instrument_id
strategy_manager.register_strategy(instrument_id, &my_strat);

// Catch-all: receives updates for every instrument
strategy_manager.register_strategy(&my_strat);
```

On each `onMarketData()` call, the manager:
1. Looks up `marketUpdate->_instrument_id` in a `std::unordered_map`
2. Calls every strategy registered for that instrument
3. Calls every catch-all strategy
4. Writes each returned signal to `LFQueue<double>`

`StrategyManager::onMarketData()` is called by `OrderBookManager` on the OBM thread immediately after each book update. Strategies must be fast — no I/O, no heap allocation.

Typical wiring in `main.cpp` (all three strategy types per symbol):
```cpp
MidPriceReversion  btc_mr(obm.book_for(btc_id));
OrderBookImbalance btc_oi(obm.book_for(btc_id));
MicroMomentum      btc_mm(obm.book_for(btc_id), binance_queue);

strategy_manager.register_strategy(btc_id, &btc_mr);
strategy_manager.register_strategy(btc_id, &btc_oi);
strategy_manager.register_strategy(btc_id, &btc_mm);
```

Each registered strategy produces one signal per `MarketUpdate`, so three signals are enqueued per update for a symbol with three registered strategies.

---

## Mean Reversion

### MidPriceReversion

**Header:** `include/strategies/mean_reversion/midprice_reversion.h`
**Source:** `src/strategies/mean_reversion/midprice_reversion.cpp`

Computes a signal based on how far the last trade price has deviated from the volume-weighted mid-price.

```
signal = (lastPrice - midPrice) / spread
```

- Clamped to `[-3, 3]`, then normalized to `[-1, 1]` by dividing by 3
- Positive signal (lastPrice above mid) → sell pressure expected → negative trade signal
- `spread = bestAsk - bestBid`

Constructor takes an `OrderBook&` bound to its symbol:
```cpp
MidPriceReversion strat(obm.book_for(btc_id));
```

### OrderBookImbalance

**Header:** `include/strategies/mean_reversion/orderbook_imbalance.h`
**Source:** `src/strategies/mean_reversion/orderbook_imbalance.cpp`

Computes the **micro-price** — a quantity-weighted average that shifts toward the side with more volume — and compares it to the volume-weighted mid-price:

```
microPrice = (askPrice × bidQty + bidPrice × askQty) / (bidQty + askQty)
signal     = (microPrice - midPrice) / spread
```

Clamped to `[-3, 3]`, then normalized to `[-1, 1]`. A positive signal means micro-price is above mid (bid-side pressure) — expect upward drift, so buy. Constructor takes an `OrderBook&`; `onMarketData()` reads top-of-book prices and quantities directly from the book after each update.

```cpp
OrderBookImbalance strat(obm.book_for(btc_id));
strategy_manager.register_strategy(btc_id, &strat);
```

---

## Trend Following

### MicroMomentum

**Header:** `include/strategies/trend_following/micro_momentum.h`
**Source:** `src/strategies/trend_following/micro_momentum.cpp`

Tracks the imbalance of BUY-side vs SELL-side depth updates over a rolling window (`kMomentumWindow = 20` updates):

```
signal = (bid_updates - ask_updates) / kMomentumWindow
```

Normalized to `[-1, 1]`. Counters reset at each window boundary. A positive signal means more bid-side activity in the current window — buy pressure. Constructor takes an `OrderBook&` and a `LFQueue<MarketUpdate>&` (bound to the venue's ingestion queue for future windowing extensions).

```cpp
MicroMomentum strat(obm.book_for(btc_id), binance_queue);
strategy_manager.register_strategy(btc_id, &strat);
```

---

## Adding a New Strategy

1. Create `include/strategies/<category>/my_strategy.h` inheriting from `Strategy`
2. Implement `onMarketData()` returning a signal in `[-1, 1]`
3. Create the corresponding `.cpp` in `src/strategies/<category>/`
4. Add to `src/strategies/CMakeLists.txt`:
   ```cmake
   add_library(MyStrategy my_strategy.cpp)
   target_include_directories(MyStrategy PUBLIC ${PROJECT_SOURCE_DIR}/include)
   ```
5. Link into `MainExec` in root `CMakeLists.txt`
6. In `main.cpp`, construct with the correct `OrderBook&` and register per-symbol:
   ```cpp
   MyStrategy strat(obm.book_for(instrument_id));
   strategy_manager.register_strategy(instrument_id, &strat);
   ```
