#pragma once

#include <market_data/order_book/market_update.h>

struct Signal
{
    int symbol_id;
    double signal_strength;
};

class Strategy
{
public:
    virtual double onMarketData(const MarketUpdate *marketUpdate) = 0;
    // virtual void onOrderUpdate(const OrderUpdate &update) = 0;
    // virtual std::vector<Order> generateOrders() = 0;
    // virtual ~Strategy() {}
};
