# Strategies Module

## Interface (`strategy.h`)

```cpp
struct Signal { int symbol_id; double signal_strength; };

class Strategy {
public:
    virtual double onMarketData(const MarketUpdate* marketUpdate) = 0;
};
```

Return value is a signal in `[-1.0, 1.0]`: -1 = strong sell, 0 = neutral, +1 = strong buy.

## StrategyManager (`strategy_manager.hpp`)

Dispatches market updates to strategies. Supports two registration modes:

```cpp
// Per-symbol: strategy only receives updates for that instrument
strategy_manager.register_strategy(instrument_id, &my_strat);

// Catch-all: receives updates for every instrument
strategy_manager.register_strategy(&my_strat);
```

`onMarketData()` is called by `OrderBookManager` after each book update (on the OBM thread). The strategy should be fast — no I/O, no allocation.

Signals are written to a `LFQueue<double>` consumed by `PCModel`.

## Available Strategies

| Class | File | Signal basis | Status |
|-------|------|-------------|--------|
| `MidPriceReversion` | `mean_reversion/midprice_reversion.h` | `(lastPrice - midPrice) / spread`, clamped, normalized | Wired |
| `OrderBookImbalance` | `mean_reversion/orderbook_imbalance.h` | Micro-price vs vol-weighted mid, spread-normalized | Wired |
| `MicroMomentum` | `trend_following/micro_momentum.h` | BUY vs SELL depth updates over 20-tick window | Wired |

Each strategy holds an `OrderBook&` reference bound at construction. The reference is stable (see `OrderBookManager` invariants in `market_data/CLAUDE.md`). All three are registered per symbol in `main.cpp` — each `MarketUpdate` produces three signals.

## Adding a New Strategy

1. Create `include/strategies/<name>.h` and `src/strategies/<name>.cpp`
2. Inherit from `Strategy`, implement `onMarketData()`
3. Add `add_library(<Name> <name>.cpp)` to `src/strategies/CMakeLists.txt`
4. Link it into `MainExec` in the root `CMakeLists.txt`
5. In `main.cpp`: construct with `OrderBook&` from `obm.book_for(id)`, register with `strategy_manager`
