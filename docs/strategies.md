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

---

## StrategyManager

**Header:** `include/strategies/strategy_manager.hpp`

Holds up to 10 strategy pointers. On each market update, calls `onMarketData()` on every registered strategy and writes the resulting signal to a `LFQueue<double>`.

```cpp
StrategyManager mgr(signals_queue);
mgr.register_strategy(&mid_reversion);
mgr.register_strategy(&obi_strategy);
mgr.onMarketData(&update);   // dispatches to all, enqueues signals
```

Currently writes one signal per strategy per update. Aggregation (e.g., averaging signals) is left to `PCModel`.

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

**Interpretation:** When price is above mid, the strategy expects mean reversion downward and generates a sell signal (and vice versa).

### OrderBookImbalance

**Header:** `include/strategies/mean_reversion/orderbook_imbalance.h`

Measures the relative imbalance between bid and ask volume near the top of book:

```
OBI = (bid_qty - ask_qty) / (bid_qty + ask_qty)
```

Result is in `[-1, 1]`. A strongly positive OBI (more bid volume than ask) suggests upward price pressure.

---

## Trend Following

### MicroMomentum

**Header:** `include/strategies/trend_following/micro_momentum.h`

Tracks aggressive order flow — the count of orders lifting the ask (aggressive buys) vs. hitting the bid (aggressive sells) over a short window.

```
momentum = (aggressive_buys - aggressive_sells) / total_aggressive_orders
```

Produces a signal in `[-1, 1]`. Positive momentum signals continued upward drift; negative signals downward.

---

## Adding a New Strategy

1. Create a header in `include/strategies/<category>/my_strategy.h` inheriting from `Strategy`
2. Implement `onMarketData()` returning a signal in `[-1, 1]`
3. Create the corresponding `.cpp` in `src/strategies/<category>/`
4. Add the source to the strategies `CMakeLists.txt`
5. Instantiate and register in `main.cpp`:
   ```cpp
   MyStrategy my_strat(order_book);
   strategy_manager.register_strategy(&my_strat);
   ```
