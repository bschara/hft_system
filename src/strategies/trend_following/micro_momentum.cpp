#include "strategies/trend_following/micro_momentum.h"
#include <algorithm>

MicroMomentum::MicroMomentum(OrderBook &ob, Common::LFQueue<MarketUpdate> &muq)
    : order_book(ob), market_update_queue(muq) {}

double MicroMomentum::getMomentumSignal(double numAggBids, double numAggAsks)
{
    return numAggBids - numAggAsks;
}

double MicroMomentum::onMarketData(const MarketUpdate *update)
{
    if (update->_side == Side::BUY) ++agg_bids_;
    else                            ++agg_asks_;
    ++window_count_;

    double signal = getMomentumSignal(agg_bids_, agg_asks_);

    if (window_count_ >= kMomentumWindow) {
        agg_bids_ = agg_asks_ = window_count_ = 0;
    }

    // Normalize: max raw value is kMomentumWindow (all updates one side)
    return std::clamp(signal / static_cast<double>(kMomentumWindow), -1.0, 1.0);
}
