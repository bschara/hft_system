#pragma once

#include "market_data/order_book/order_book.h"
#include "strategies/strategy.h"

class MidPriceReversion : public Strategy
{
public:
    MidPriceReversion(OrderBook &ob);

    // > 0 short |||| < 0 long
    double generate_reversion_signal(double last_trade_price);

    double onMarketData(const MarketUpdate *marketUpdate) override;

private:
    OrderBook &order_book;
};