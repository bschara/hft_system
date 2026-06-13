#pragma once

#include "market_data/order_book/order_book.h"
#include "strategies/strategy.h"

class OrderBookImbalance : public Strategy
{
public:
    OrderBookImbalance(OrderBook &ob);

    // Micro-price vs volume-weighted mid; positive → sell, negative → buy
    double generate_obi(double askPrice, double askQuantity, double bidPrice, double bidQuantity);

    double onMarketData(const MarketUpdate *marketUpdate) override;

private:
    OrderBook &order_book;
};
