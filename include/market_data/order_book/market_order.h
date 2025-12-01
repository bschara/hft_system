#ifndef MARKET_ORDER_H
#define MARKET_ORDER_H

#include <stdint.h>

enum Side
{
    BUY,
    SELL,
    INVALID
};

enum OrderType
{
    LIMIT,
    MARKET,
    STOP,
};

struct SimplifiedOrder
{
    double price;
    double quantity;
};

struct __attribute__((aligned(64))) Order
{
    long long orderID;
    double quantity;
    double price;
    Side side = Side::INVALID;
    // OrderType type;
    uint64_t timestamp;
    Order *head;
    Order *tail;

    Order() = default;

    // Order(long long _orderID, double _quantity, double _price, Side _side, OrderType _type, uint64_t _timestamp,
    // Order* head = nullptr, Order* tail = nullptr) : orderID(_orderID), quantity(_quantity), price(_price),
    // side(_side), type(_type), timestamp(_timestamp){};

    Order(long long _orderID, double _quantity, double _price, Side _side, uint64_t _timestamp,
          Order *head = nullptr, Order *tail = nullptr) : orderID(_orderID), quantity(_quantity), price(_price),
                                                          side(_side), timestamp(_timestamp) {};
};

struct MarketOrderAtPrice
{
    Side _side = Side::INVALID;
    float price;

    Order *firstOrder;

    MarketOrderAtPrice *next;
    MarketOrderAtPrice *prev;

    MarketOrderAtPrice() = default;

    MarketOrderAtPrice(Side s, float p, Order *o, std::nullptr_t n1 = nullptr, std::nullptr_t n2 = nullptr)
        : _side(s), price(p), firstOrder(o)
    {
    }
};

#endif