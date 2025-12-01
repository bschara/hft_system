#ifndef MARKET_UPDATE_H
#define MARKET_UPDATE_H

#include <iostream>
#include "market_order.h"
#include <vector>
#include <cmath>

enum class MarketUpdateType : uint8_t
{
    INVALID = 0,
    CLEAR = 1,
    ADD = 2,
    MODIFY = 3,
    CANCEL = 4,
    TRADE = 5,
    SNAPSHOT_START = 6,
    SNAPSHOT_END = 7
};

struct BestBidAskEvent
{
    int64_t eventTime;
    int64_t bookUpdateId;
    int8_t priceExponent;
    int8_t qtyExponent;
    int64_t bidPrice;
    int64_t bidQty;
    int64_t askPrice;
    int64_t askQty;
    std::string symbol;

    double scaledBidPrice() const
    {
        return static_cast<double>(bidPrice) * std::pow(10.0, priceExponent);
    }

    double scaledBidQty() const
    {
        return static_cast<double>(bidQty) * std::pow(10.0, qtyExponent);
    }

    double scaledAskPrice() const
    {
        return static_cast<double>(askPrice) * std::pow(10.0, priceExponent);
    }

    double scaledAskQty() const
    {
        return static_cast<double>(askQty) * std::pow(10.0, qtyExponent);
    }
};

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

struct DepthUpdate
{
    // std::string event;
    // uint64_t eventTime;
    uint64_t transactionTime;
    std::string symbol;
    double bidPrice;
    double askPrice;
    double askQuantity;
    double bidQuantity;
};

#endif