#ifndef MARKET_UPDATE_H
#define MARKET_UPDATE_H

#include <iostream>
#include "market_order.h"
#include <vector>
#include <cmath>

struct MarketUpdate
{
    int64_t _symbol;
    Side _side;
    int64_t _price;
    int64_t _quantity;
    int64_t _timestamp;

    MarketUpdate() = default;

    MarketUpdate(int64_t symbol, Side side,
                 int64_t price, int64_t quantity, int64_t timestamp) : _symbol(symbol), _side(side), _price(_price), _quantity(_quantity), _timestamp(timestamp)
    {
    }
};

#endif