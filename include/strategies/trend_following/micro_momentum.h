#pragma once

#include "market_data/order_book/order_book.h"
#include "strategies/strategy.h"
#include "utils/lock_free_queue.hpp"

// for crypto use window of 5-30 ticks
class MicroMomentum : public Strategy
{
public:
    static constexpr uint32_t kMomentumWindow = 20;

    MicroMomentum(OrderBook &ob, Common::LFQueue<MarketUpdate> &muq);

    // positive and increasing -> go long |||| negative and decreasing -> go short
    double getMomentumSignal(double numAggBids, double numAggAsks);

    double onMarketData(const MarketUpdate *marketUpdate) override;

private:
    OrderBook                       &order_book;
    Common::LFQueue<MarketUpdate>   &market_update_queue;

    uint32_t agg_bids_     = 0;
    uint32_t agg_asks_     = 0;
    uint32_t window_count_ = 0;
};
