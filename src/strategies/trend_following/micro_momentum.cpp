#include "strategies/trend_following/micro_momentum.h"
#include <algorithm>

MicroMomentum::MicroMomentum(OrderBook &ob) : order_book(ob) {}

int32_t MicroMomentum::onMarketData(const MarketUpdate *update)
{
    if (update->_side == Side::BUY) ++agg_bids_;
    else                            ++agg_asks_;
    ++window_count_;

    // Signal = (aggBids - aggAsks) * 1000 / window, clamped to [-1000, 1000]
    int32_t signal = static_cast<int32_t>(
        std::clamp(((int64_t)agg_bids_ - (int64_t)agg_asks_) * 1000 /
                   (int64_t)kMomentumWindow,
                   (int64_t)-1000, (int64_t)1000));

    if (window_count_ >= kMomentumWindow)
        agg_bids_ = agg_asks_ = window_count_ = 0;

    return signal;
}
