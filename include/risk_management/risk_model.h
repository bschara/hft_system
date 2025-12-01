#pragma once

#include "pcm_model/pcm_model.h"

struct RiskLimits
{
    double max_position_per_instrument;
    double max_notional_exposure;
    double max_total_risk; // e.g., max portfolio volatility or VaR
    double max_leverage;
    double max_trade_size;
};

struct TradeIntent
{
    double price;
    Side side;
    double size;
};

enum class RiskStatus
{
    OK,
    PositionLimitExceeded,
    NotionalLimitExceeded,
    CapitalLimitExceeded,
    VaRLimitExceeded
};

class RiskModel
{
    RiskStatus checkPreTradeRisk(TradeIntent &intent);
};