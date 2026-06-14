#pragma once

#include <cstdint>
#include <cstddef>
#include <span>
#include <string_view>
#include "message_schema.h"
#include "market_data/order_book/market_update.h"
#include "market_data/venue_registry.hpp"
#include "utils/containers/lock_free_queue.hpp"

class SBEDecoder
{
public:
    // Decode one SBE depth message. Enqueues raw-integer MarketUpdates.
    // Returns bytes consumed, or 0 on error.
    static size_t decode(std::span<const uint8_t>      msg,
                         const MessageSchema           &schema,
                         Common::LFQueue<MarketUpdate> &queue,
                         const VenueRegistry           &registry,
                         std::string_view               venue_name);

    // Compute bytes consumed by one SBE message without enqueuing anything.
    // Used by SBEVenueParser to skip messages with schema.skip == true.
    static size_t measure(std::span<const uint8_t> msg, const MessageSchema &schema);
};
