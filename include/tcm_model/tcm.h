
// bid ask spread cost
// only for market orders
// spread_cost = (spread / 2) * trade_volume

// slippage
// slippage_cost = (actual_execution_price - expected_price) * trading_volume
// increases with volatility latency and aggressive execution

// market_impact
// market_impact_cost = I * (Q / ADV)^alfa * volatility * price_of_asset
// Q -> trade_size / ADV -> average daily volume of asset
// I -> market impact coefficient
// alpha usually between 0.5 and 1

// total_cost = sum (volume(i), bucket_size(i) * Rate(i))
// volume -> the remaining volume applicable to the current bucket
//  bucket_size -> maximum volume allowed per cost bucket
// rate -> cost per unit volume for that bucket

// total_cost = commission + spread_cost + slippage + market_impact

class TransactionCostModel
{

public:
    TransactionCostModel(double _exchange_fees, double _latency);

    double calculate_spread_cost(double spread, double trade_volume);

    double estimate_slippage_cost(
        double trade_volume,
        double volatility,
        double spread,
        double top_book_depth,
        double price,
        double aggressiveness);

    double estimate_market_impact_cost(
        double trade_size,
        double adv,
        double volatility,
        double price,
        double I = 0.1,
        double alpha = 0.6);

    double computeTotalCost(double trade_volume, double spread, double volatility,
                            double top_book_depth, double price, double aggressiveness,
                            double trade_size,
                            double adv, double I = 0.1,
                            double alpha = 0.6);

private:
    int exchange_fees;
    double latency = 0;
    int trade_size[4] = {1, 5, 10, 11}; // % ADV
};