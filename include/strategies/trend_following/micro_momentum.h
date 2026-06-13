#pragma once

#include "market_data/order_book/order_book.h"
#include "strategies/strategy.h"

class MicroMomentum : public Strategy
{
public:
    static constexpr uint32_t kMomentumWindow = 20;

    explicit MicroMomentum(OrderBook &ob);

    int32_t onMarketData(const MarketUpdate *marketUpdate) override;

private:
    OrderBook &order_book;
    uint32_t   agg_bids_     = 0;
    uint32_t   agg_asks_     = 0;
    uint32_t   window_count_ = 0;
};
