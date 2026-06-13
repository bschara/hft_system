#pragma once

#include <cstdint>
#include <cstddef>
#include <span>
#include <string_view>
#include "message_schema.h"
#include "market_data/order_book/market_update.h"
#include "market_data/venue_registry.hpp"
#include "utils/lock_free_queue.hpp"

class SBEDecoder
{
public:
    // Decode one SBE message from `msg`. Enqueues raw-integer MarketUpdates.
    // Returns bytes consumed (total size of this SBE message), or 0 on error.
    static size_t decode(std::span<const uint8_t>      msg,
                         const MessageSchema           &schema,
                         Common::LFQueue<MarketUpdate> &queue,
                         const VenueRegistry           &registry,
                         std::string_view               venue_name);
};
