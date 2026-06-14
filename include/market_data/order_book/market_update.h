#pragma once

#include "market_order.h"
#include <cstdint>

struct alignas(64) MarketUpdate
{
    uint32_t _instrument_id;
    Side     _side;
    int64_t  _price;        // raw integer from exchange
    int64_t  _quantity;     // raw integer (0 = remove level)
    int8_t   _price_exp;    // actual price = _price × 10^_price_exp
    int8_t   _qty_exp;      // actual qty   = _quantity × 10^_qty_exp
    int64_t  _timestamp;    // exchange event time (microseconds)
    uint64_t _recv_tsc = 0;

    MarketUpdate() = default;

    MarketUpdate(uint32_t instrument_id, Side side,
                 int64_t price, int64_t quantity,
                 int8_t price_exp, int8_t qty_exp,
                 int64_t timestamp)
        : _instrument_id(instrument_id), _side(side),
          _price(price), _quantity(quantity),
          _price_exp(price_exp), _qty_exp(qty_exp),
          _timestamp(timestamp), _recv_tsc(0)
    {}
};
