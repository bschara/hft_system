#include "pcm_model/pcm_model.h"

PCModel::PCModel(double _capital, Common::LFQueue<double> *_signals, OrderBook &_order_book, TransactionCostModel &_tcm,
                 RiskModel &_risk_model)
    : initialCapital(_capital), availableCapital(_capital), signals(*_signals), order_book(_order_book), tcm(_tcm),
      risk_model(_risk_model) {}

TradeIntent PCModel::generatetradeIntent(double signal)
{
    std::cout << "signal " << signal << std::endl;
    Side direction = (signal > 0) ? Side::BUY : Side::SELL;
    double strength = std::min(std::abs(signal), 3.0) / 3.0;
    std::cout << "strength " << strength << std::endl;

    double capital = this->availableCapital;
    double mid_price = order_book.getMidPrice();

    double allocation = strength * capital_fraction;
    double notional = capital * allocation;
    double size = notional / mid_price;

    TradeIntent t{mid_price, direction, size};
    return t;
}

// TradeIntent PCModel::generatetradeIntent(double signal)
// {
//     Side direction = (signal > 0) ? Side::BUY : Side::SELL;
//     double strength = std::min(std::abs(signal), 3.0) / 3.0;

//     double capital = availableCapital;
//     double mid_price = order_book.getMidPrice();

//     // --- Step 1: Determine initial desired notional ---
//     double allocation = strength * capital_fraction;
//     double notional = capital * allocation;
//     double size = notional / mid_price;

//     // --- Step 2: Estimate transaction costs ---
//     double spread = order_book.getSpread();
//     double volatility = order_book.getShortTermVolatility();
//     double top_depth = order_book.getTopBookDepth();

//     double est_cost = tcm.computeTotalCost(
//         size,
//         spread,
//         volatility,
//         top_depth,
//         mid_price,
//         /* aggressiveness */ 1.0,
//         size,
//         /* adv */ 100.0 * top_depth, // or from market stats
//         0.1,
//         0.6);

//     // --- Step 3: Adjust for risk ---
//     bool allowed = risk_model.checkTradeRisk(direction, size, mid_price, est_cost);
//     if (!allowed)
//     {
//         std::cerr << "[RiskModel] Trade blocked due to exposure or cost limits." << std::endl;
//         return TradeIntent{mid_price, Side::NONE, 0.0};
//     }

//     // --- Step 4: Adjust position sizing for cost ---
//     double effective_size = std::max(0.0, size - (est_cost / mid_price));
//     TradeIntent t{mid_price, direction, effective_size};

//     std::cout << "TradeIntent: price=" << mid_price
//               << " side=" << (direction == Side::BUY ? "BUY" : "SELL")
//               << " size=" << effective_size
//               << " estCost=" << est_cost << std::endl;

//     return t;
// }

void PCModel::run()
{
    if (running == false)
    {
        running = true;
    }

    while (running)
    {
        auto signal = signals.getNextToRead();
        if (signal)
        {
            TradeIntent t = generatetradeIntent(*signal);
            // std::cerr << "price " << t.price << " side " << t.side << " size " << t.size << std::endl;
            signals.updateReadIndex();
        }
    }
}