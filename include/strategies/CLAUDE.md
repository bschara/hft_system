# Strategies Module

## Interface (`strategy.h`)

```cpp
struct Signal { int symbol_id; int32_t signal_strength; };  // [-1000, 1000]

class Strategy {
public:
    virtual int32_t onMarketData(const MarketUpdate* marketUpdate) = 0;
};
```

Return value is a signal in `[-1000, 1000]`: -1000 = max sell, 0 = neutral, +1000 = max buy. All strategy math is pure integer arithmetic — no floating-point on the hot path. `PCModel` is the single place that divides by 1000 to normalize for position sizing.

## StrategyManager (`strategy_manager.hpp`)

Dispatches market updates to strategies. Supports two registration modes:

```cpp
// Per-symbol: strategy only receives updates for that instrument
strategy_manager.register_strategy(instrument_id, &my_strat);

// Catch-all: receives updates for every instrument
strategy_manager.register_strategy(&my_strat);
```

`onMarketData()` is called by `OrderBookManager` after each book update (on the OBM thread). The strategy should be fast — no I/O, no allocation.

Signals are written to a `LFQueue<int32_t>` consumed by `PCModel`.

## Available Strategies

| Class | File | Signal basis | Signal math |
|-------|------|-------------|-------------|
| `MidPriceReversion` | `mean_reversion/midprice_reversion.h` | `(price - mid) * 1000 / (3 * spread)`, clamped | pure int64 |
| `OrderBookImbalance` | `mean_reversion/orderbook_imbalance.h` | `(microPrice - mid) * 1000 / (3 * spread)` | `__int128` for price×qty products |
| `MicroMomentum` | `trend_following/micro_momentum.h` | `(aggBids - aggAsks) * 1000 / window` over 20-update window | pure int64 |

Each strategy holds an `OrderBook&` reference bound at construction. The reference is stable (see `OrderBookManager` invariants in `market_data/CLAUDE.md`). All three are registered per symbol in `main.cpp` — each `MarketUpdate` produces three signals.

`OrderBookImbalance` uses `__int128` for the intermediate `askPrice * bidQty + bidPrice * askQty` product to prevent int64 overflow on large raw integers.

## Adding a New Strategy

1. Create `include/strategies/<name>.h` and `src/strategies/<name>.cpp`
2. Inherit from `Strategy`, implement `int32_t onMarketData(const MarketUpdate*)` returning a value in `[-1000, 1000]`
3. Add `add_library(<Name> <name>.cpp)` to `src/strategies/CMakeLists.txt`
4. Link it into `MainExec` in the root `CMakeLists.txt`
5. In `main.cpp`: construct with `OrderBook&` from `obm.book_for(id)`, register with `strategy_manager`
