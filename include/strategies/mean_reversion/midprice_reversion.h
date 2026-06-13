#pragma once

#include "market_data/order_book/order_book.h"
#include "strategies/strategy.h"

class MidPriceReversion : public Strategy
{
public:
    explicit MidPriceReversion(OrderBook &ob);

    int32_t onMarketData(const MarketUpdate *marketUpdate) override;

private:
    OrderBook &order_book;
};
