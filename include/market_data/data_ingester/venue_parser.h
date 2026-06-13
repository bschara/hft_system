#pragma once

#include <cstdint>
#include <span>
#include "market_data/order_book/market_update.h"
#include "utils/lock_free_queue.hpp"

class VenueParser
{
public:
    virtual void parse(std::span<const uint8_t>       payload,
                       Common::LFQueue<MarketUpdate>  &queue) = 0;
    virtual ~VenueParser() = default;
};
