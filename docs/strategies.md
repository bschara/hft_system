# Strategies

## Strategy Interface

**Header:** `include/strategies/strategy.h`

All strategies implement one virtual method:

```cpp
class Strategy {
public:
    virtual int32_t onMarketData(const MarketUpdate*) = 0;
};
```

Return value is a **signal** in `[-1000, 1000]` (integer):
- `+1000` — maximum buy conviction
- `-1000` — maximum sell conviction
- `0` — no edge / flat

All arithmetic inside strategies is pure integer — **no floating-point on the strategy hot path**. `PCModel` is the only component that converts: `strength = signal / 1000.0`.

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
4. Writes each returned `int32_t` signal to `LFQueue<int32_t>`

`StrategyManager::onMarketData()` is called by `OrderBookManager` on the OBM thread immediately after each book update. Strategies must be fast — no I/O, no heap allocation.

Typical wiring in `main.cpp` (all three strategy types per symbol):
```cpp
MidPriceReversion  btc_mr(obm.book_for(btc_id));
OrderBookImbalance btc_oi(obm.book_for(btc_id));
MicroMomentum      btc_mm(obm.book_for(btc_id));

strategy_manager.register_strategy(btc_id, &btc_mr);
strategy_manager.register_strategy(btc_id, &btc_oi);
strategy_manager.register_strategy(btc_id, &btc_mm);
```

Each registered strategy produces one signal per `MarketUpdate`, so three `int32_t` signals are enqueued per update for a symbol with three registered strategies.

---

## Mean Reversion

### MidPriceReversion

**Header:** `include/strategies/mean_reversion/midprice_reversion.h`
**Source:** `src/strategies/mean_reversion/midprice_reversion.cpp`

Computes a signal based on how far the last trade price deviates from the volume-weighted mid-price. Pure integer arithmetic:

```
deviation = (price - midPrice) * 1000 / (3 * spread)
signal    = clamp(deviation, -1000, 1000)
```

- `price` — raw integer from `update->_price`
- `midPrice` — from `order_book.getMidRaw()`
- `spread = bestAsk - bestBid` (raw integers)
- `3 * spread` normalises to a ±3-spread window, matching the old float convention

Constructor takes an `OrderBook&` bound to its symbol:
```cpp
MidPriceReversion strat(obm.book_for(btc_id));
```

### OrderBookImbalance

**Header:** `include/strategies/mean_reversion/orderbook_imbalance.h`
**Source:** `src/strategies/mean_reversion/orderbook_imbalance.cpp`

Computes the **micro-price** — a quantity-weighted average that shifts toward the side with more volume — and compares it to the volume-weighted mid-price:

```
micro128   = (int128)(askPrice × bidQty) + (int128)(bidPrice × askQty)
microPrice = micro128 / (bidQty + askQty)
signal     = clamp((microPrice - midPrice) * 1000 / (3 * spread), -1000, 1000)
```

`__int128` is used for the intermediate `price × qty` products to prevent `int64_t` overflow on large raw integers (e.g. BTCUSDT raw prices × raw quantities). All other arithmetic is `int64_t`.

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
signal = clamp((aggBids - aggAsks) * 1000 / kMomentumWindow, -1000, 1000)
```

Counters reset at each window boundary. A positive signal means more bid-side depth updates in the current window — buy pressure. Pure integer arithmetic.

Constructor takes only an `OrderBook&` (the queue parameter was removed — it was unused):
```cpp
MicroMomentum strat(obm.book_for(btc_id));
strategy_manager.register_strategy(btc_id, &strat);
```

---

## Adding a New Strategy

1. Create `include/strategies/<category>/my_strategy.h` inheriting from `Strategy`
2. Implement `int32_t onMarketData(const MarketUpdate*)` returning a value in `[-1000, 1000]`; use integer arithmetic throughout; use `__int128` if intermediate products risk overflow
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
