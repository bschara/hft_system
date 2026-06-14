# Risk Management & Position Control

## Transaction Cost Model (TCM)

**Header:** `include/tcm_model/tcm.h`
**Source:** `src/tcm_model/tcm.cpp`

Estimates the full cost of executing a trade before submission. Three cost components are summed:

### Spread Cost

```
spread_cost = (spread / 2) × volume
```

The half-spread represents the immediate cost of crossing the bid-ask spread.

### Slippage Cost

```
slippage_cost = f(volatility, latency, book_depth, order_size)
```

Estimates adverse price movement between decision and execution, driven by:
- Market volatility
- Execution latency
- Available liquidity at the top of book

### Market Impact Cost

```
impact_cost = I × (trade_size / ADV)^alpha
```

| Parameter | Default | Meaning |
|---|---|---|
| `I` | 0.1 | Market impact coefficient |
| `alpha` | 0.6 | Size elasticity (sub-linear impact, square-root law) |
| `ADV` | per-symbol | Average Daily Volume |

### Total Cost

```cpp
double total = tcm.computeTotalCost(spread, volume, volatility,
                                    latency, book_depth, trade_size, ADV);
```

---

## Risk Model

**Header:** `include/risk_management/risk_model.h`

### Risk Limits

```cpp
struct RiskLimits {
    double max_position_per_instrument;
    double max_notional_exposure;
    double max_total_risk;
    double max_leverage;
    double max_trade_size;
};
```

### Pre-Trade Check

`RiskModel::checkPreTradeRisk(TradeIntent, current_position, capital)` returns a `RiskStatus`:

| Status | Meaning |
|---|---|
| `OK` | Trade passes all checks |
| `PositionLimitExceeded` | Would breach per-instrument position cap |
| `NotionalLimitExceeded` | Would breach notional exposure limit |
| `LeverageExceeded` | Would breach max leverage |
| `TradeSizeExceeded` | Single trade too large |

Trades failing pre-trade risk are rejected before reaching the order gateway.

---

## Position / Capital Model (PCModel)

**Header:** `include/pcm_model/pcm_model.h`
**Source:** `src/pcm_model/pcm_model.cpp`

Converts an integer signal into a concrete `TradeIntent` (price, side, size). This is the **only place in the pipeline where a float conversion occurs**:

### Signal → Size Mapping

```cpp
// signal is int32_t in [-1000, 1000]
double strength   = std::min(std::abs(signal), 1000) / 1000.0;  // → [0.0, 1.0]
double allocation = strength * capital_fraction;                  // default: 0.02 (2%)
double size       = (capital * allocation) / mid_price;
```

- `capital_fraction = 0.02` limits each trade to at most 2% of available capital
- `mid_price` comes from `order_book.getMidPrice()` (which internally calls `PriceUtils::to_double(getMidRaw(), price_exp_)`)
- Size is denominated in base asset units
- Direction: signal > 0 → BUY, signal < 0 → SELL

### Signal Queue

PCModel consumes `LFQueue<int32_t>` (not `double`). Signals from all strategies are enqueued as raw integers; PCModel is the only component that converts to float.

### TradeIntent

```cpp
struct TradeIntent {
    double price;
    Side   side;
    double size;
};
```

### Extended Sizing (Commented Out)

A more complete version adjusting for transaction costs and risk score is stubbed in `pcm_model.cpp`. It modifies allocation based on:
- `tcm.computeTotalCost()` — reduces size when costs are high
- `risk_model.checkPreTradeRisk()` — blocks or scales down if limits would be breached

This is the planned production path once OMS and order gateway are wired.

---

## TradeIntent Flow

```
signal (int32_t, [-1000, 1000])
    ↓ PCModel::generatetradeIntent()  — divides by 1000 (only float conversion)
TradeIntent { price, side, size }
    ↓ RiskModel::checkPreTradeRisk()          [TODO: integrate]
    ↓ TCModel::computeTotalCost()             [TODO: integrate]
    ↓ OMS (stub)
    ↓ OrderGateway → Exchange
```
