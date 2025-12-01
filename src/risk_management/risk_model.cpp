#include "risk_management/risk_model.h"

RiskResult RiskModel::preTradeCheck(const TradeIntent &intent,
                                    const double available_capital,
                                    const MarketData &market)
{
    if (abs(portfolio.getPosition(intent.instrument) + intent.size) > limits_.max_position_per_instrument)
        return RiskResult::rejected("Position limit exceeded");

    double notional = std::abs(intent.size * intent.price);
    if (notional > limits_.max_notional_per_instrument)
        return RiskStatus::NotionalLimitExceeded;

    if (notional > available_capital)
        return RiskStatus::CapitalLimitExceeded;

    if (std::abs(intent.size) > limits_.max_trade_size)
        return RiskStatus::PositionLimitExceeded;

    return RiskResult::ok();
}
