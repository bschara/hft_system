#include "tcm_model/tcm.h"

TransactionCostModel::TransactionCostModel(double _exchange_fees, double _latency) : exchange_fees(_exchange_fees),
                                                                                     latency(_latency) {
                                                                                     };

double TransactionCostModel::estimate_slippage_cost(double trade_volume,
                                                    double volatility,
                                                    double spread,
                                                    double top_book_depth,
                                                    double price,
                                                    double aggressiveness)
{
    double vol_component = volatility * sqrt(latency);
    double liquidity_component = (trade_volume / top_book_depth) * spread;
    double base_slippage = vol_component + liquidity_component;

    double slippage_per_unit = base_slippage * aggressiveness;
    return slippage_per_unit * trade_volume;
};

double TransactionCostModel::estimate_market_impact_cost(double trade_size,
                                                         double adv,
                                                         double volatility,
                                                         double price,
                                                         double I = 0.1,
                                                         double alpha = 0.6)
{
    if (adv <= 0.0)
        return 0.0;

    double normalized_size = trade_size / adv;
    double impact = I * pow(normalized_size, alpha) * volatility * price;
    return impact * trade_size;
};

double TransactionCostModel::calculate_spread_cost(double spread, double trade_volume)
{
    double temp = spread / 2;
    return temp * trade_volume;
};

double TransactionCostModel::computeTotalCost(double trade_volume, double spread, double volatility,
                                              double top_book_depth, double price, double aggressiveness, double trade_size, double adv, double I,
                                              double alpha)
{

    double spread_cost = calculate_spread_cost(spread, trade_volume);

    double commission_fees = price * trade_volume * (exchange_fees / 100.0);

    double slippage_cost = estimate_slippage_cost(trade_volume,
                                                  volatility,
                                                  spread,
                                                  top_book_depth,
                                                  price,
                                                  aggressiveness);

    double market_impact_cost = estimate_market_impact_cost(trade_size,
                                                            adv,
                                                            volatility,
                                                            price,
                                                            I,
                                                            alpha);

    return commission_fees + spread_cost + market_impact_cost + slippage_cost;
};