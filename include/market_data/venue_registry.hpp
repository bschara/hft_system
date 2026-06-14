#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include "utils/config/stream_config/stream_config.h"

// Maps (exchange, symbol) pairs to a packed uint32_t instrument_id.
// bits[31:16] = venue index, bits[15:0] = symbol index within that venue.
// Populated once at startup from StreamConfig rows; read-only on the hot path.
class VenueRegistry
{
public:
    explicit VenueRegistry(const std::vector<StreamRow> &rows)
    {
        for (const auto &r : rows)
        {
            uint16_t vid = get_or_add_venue(r.exchange);
            uint16_t sid = next_symbol_idx_[vid]++;
            instrument_map_[r.exchange + ":" + r.symbol] = pack(vid, sid);
        }
    }

    uint32_t lookup(std::string_view exchange, std::string_view symbol) const
    {
        std::string key = std::string(exchange) + ":" + std::string(symbol);
        auto it = instrument_map_.find(key);
        if (it == instrument_map_.end())
            throw std::runtime_error("VenueRegistry: unknown instrument " + key);
        return it->second;
    }

    uint32_t size() const { return static_cast<uint32_t>(instrument_map_.size()); }

    static constexpr uint16_t venue_of(uint32_t id) { return static_cast<uint16_t>(id >> 16); }
    static constexpr uint16_t symbol_of(uint32_t id) { return static_cast<uint16_t>(id & 0xFFFF); }

private:
    std::unordered_map<std::string, uint32_t> instrument_map_;
    std::unordered_map<std::string, uint16_t> venue_ids_;
    std::unordered_map<uint16_t, uint16_t>    next_symbol_idx_;

    static constexpr uint32_t pack(uint16_t venue, uint16_t symbol)
    {
        return (static_cast<uint32_t>(venue) << 16) | symbol;
    }

    uint16_t get_or_add_venue(const std::string &exchange)
    {
        auto [it, inserted] = venue_ids_.emplace(exchange, static_cast<uint16_t>(venue_ids_.size()));
        if (inserted)
            next_symbol_idx_[it->second] = 0;
        return it->second;
    }
};
