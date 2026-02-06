#ifndef MARKET_UPDATE_H
#define MARKET_UPDATE_H

#include <iostream>
#include "market_order.h"
#include <vector>
#include <cmath>

struct MarketUpdate
{
    // MarketUpdateType _type;
    // long long _orderID;
    Side _side;
    // double _price;
    int64_t _price;
    // double _quantity;
    int64_t _quantity;
    // uint32_t _tickerID;
    // OrderType  _orderType;
    int64_t _timestamp;

    MarketUpdate() = default;

    // MarketUpdate(MarketUpdateType _updateType, long long _orderID, Side _side,
    //              float _price, double _quantity, uint64_t timestamp) : _type(_updateType),
    //                                                                    _orderID(_orderID), _side(_side), _price(_price), _quantity(_quantity),
    //                                                                    _timestamp(timestamp) {}
    MarketUpdate(Side _side,
                 int64_t _price, int64_t _quantity, int64_t timestamp) : _side(_side), _price(_price), _quantity(_quantity), _timestamp(timestamp)
    {
    }
};

#endif