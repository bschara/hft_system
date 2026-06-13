#ifndef MARKET_UPDATE_H
#define MARKET_UPDATE_H

#include "market_order.h"
#include <cstdint>

struct alignas(64) MarketUpdate
{
    uint32_t _instrument_id; // packed: bits[31:16]=venue_idx, bits[15:0]=symbol_idx
    Side     _side;
    double   _price;
    double   _quantity;
    int64_t  _timestamp;     // exchange event time (microseconds)
    uint64_t _recv_tsc = 0;  // rdtsc at ingester enqueue time

    MarketUpdate() = default;

    MarketUpdate(uint32_t instrument_id, Side side,
                 double price, double quantity, int64_t timestamp)
        : _instrument_id(instrument_id), _side(side),
          _price(price), _quantity(quantity), _timestamp(timestamp), _recv_tsc(0)
    {
    }
};

#endif