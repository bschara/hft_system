#include "pcm_model/pcm_model.h"
#include <algorithm>

PCModel::PCModel(double _capital, Common::LFQueue<int32_t> *_signals, OrderBook &_order_book,
                 TransactionCostModel &_tcm, RiskModel &_risk_model)
    : initialCapital(_capital), availableCapital(_capital), signals(*_signals),
      order_book(_order_book), tcm(_tcm), risk_model(_risk_model)
{}

TradeIntent PCModel::generatetradeIntent(int32_t signal)
{
    Side   direction = (signal > 0) ? Side::BUY : Side::SELL;
    // Single float conversion: divide by 1000 to normalise [-1000,1000] → [-1,1]
    double strength  = std::min(std::abs(signal), 1000) / 1000.0;

    double capital   = availableCapital;
    double mid_price = order_book.getMidPrice();

    double allocation = strength * capital_fraction;
    double notional   = capital * allocation;
    double size       = (mid_price > 0.0) ? notional / mid_price : 0.0;

    return TradeIntent{mid_price, direction, size};
}

void PCModel::run()
{
    if (!running) running = true;

    while (running)
    {
        auto signal = signals.getNextToRead();
        if (signal)
        {
            generatetradeIntent(*signal);
            signals.updateReadIndex();
        }
    }
}
