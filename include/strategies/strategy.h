#pragma once

#include <cstdint>
#include <market_data/order_book/market_update.h>

struct Signal
{
    int     symbol_id;
    int32_t signal_strength; // [-1000, 1000]
};

class Strategy
{
public:
    virtual int32_t onMarketData(const MarketUpdate *marketUpdate) = 0;
};
