#pragma once

#include <unordered_map>
#include "order_book.h"
#include <libpq-fe.h>

using BinarySymbol = uint64_t;
constexpr int AMOUNT_OF_SYMBOLS = 3;

class OrderBookManager
{
public:
    explicit OrderBookManager(/* args */);
    ~OrderBookManager();

private:
    std::unordered_map<BinarySymbol, uint16_t> symbol_to_index;
    std::array<OrderBook *, AMOUNT_OF_SYMBOLS> order_books;
    PGconn &conn;
};
