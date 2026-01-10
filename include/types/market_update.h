#pragma once

enum Side
{
    BUY,
    SELL,
    INVALID
};

struct MarketUpdate
{
    Side _side;
    int64_t _price;
    int64_t _quantity;
    int64_t _timestamp;
    MarketUpdate() = default;
    MarketUpdate(Side _side,
                 int64_t _price, int64_t _quantity, int64_t timestamp) : _side(_side), _price(_price), _quantity(_quantity), _timestamp(timestamp)
    {
    }
};
