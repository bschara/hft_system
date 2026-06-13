#include "market_data/data_ingester/sbe_decoder.h"
#include "utils/benchmark/benchmark_utility.hpp"
#include <cstring>
#include <iostream>

namespace {

template <typename T>
T read_le(std::span<const uint8_t> buf, size_t offset)
{
    if (offset + sizeof(T) > buf.size()) return T{};
    T v{};
    std::memcpy(&v, buf.data() + offset, sizeof(T));
    return v;
}

int64_t read_scaled(std::span<const uint8_t> buf, size_t offset, IntEncoding enc)
{
    switch (enc) {
        case IntEncoding::INT8:   return static_cast<int64_t>(read_le<int8_t>(buf, offset));
        case IntEncoding::INT16:  return static_cast<int64_t>(read_le<int16_t>(buf, offset));
        case IntEncoding::INT32:  return static_cast<int64_t>(read_le<int32_t>(buf, offset));
        case IntEncoding::INT64:  return read_le<int64_t>(buf, offset);
        case IntEncoding::UINT8:  return static_cast<int64_t>(read_le<uint8_t>(buf, offset));
        case IntEncoding::UINT16: return static_cast<int64_t>(read_le<uint16_t>(buf, offset));
        case IntEncoding::UINT32: return static_cast<int64_t>(read_le<uint32_t>(buf, offset));
        case IntEncoding::UINT64: return static_cast<int64_t>(read_le<uint64_t>(buf, offset));
    }
    return 0;
}

} // namespace

size_t SBEDecoder::decode(std::span<const uint8_t>      msg,
                          const MessageSchema           &schema,
                          Common::LFQueue<MarketUpdate> &queue,
                          const VenueRegistry           &registry,
                          std::string_view               venue_name)
{
    // SBE header is 8 bytes: blockLength(u16), templateId(u16), schemaId(u16), version(u16)
    if (msg.size() < 8) return 0;

    size_t fixed_start = 8;
    if (fixed_start + schema.fixed_block_size > msg.size()) return 0;

    // --- Fixed block ---
    int64_t raw_ts = read_le<int64_t>(msg, fixed_start + schema.event_time_offset);
    int64_t event_time_us = raw_ts;
    if (schema.event_time_unit == TimestampUnit::MILLISECONDS)
        event_time_us = raw_ts * 1000;
    else if (schema.event_time_unit == TimestampUnit::NANOSECONDS)
        event_time_us = raw_ts / 1000;

    int8_t price_exp = read_le<int8_t>(msg, fixed_start + schema.price_exp_offset);
    int8_t qty_exp   = read_le<int8_t>(msg, fixed_start + schema.qty_exp_offset);

    size_t offset = fixed_start + schema.fixed_block_size;

    // --- Pre-scan: locate symbol field to get instrument_id before entering loops ---
    if (offset + 4 > msg.size()) return 0;
    uint16_t bidBlockLen = read_le<uint16_t>(msg, offset);
    uint16_t numBids     = read_le<uint16_t>(msg, offset + 2);
    size_t   bids_end    = offset + 4 + static_cast<size_t>(bidBlockLen) * numBids;

    if (bids_end + 4 > msg.size()) return 0;
    uint16_t askBlockLen = read_le<uint16_t>(msg, bids_end);
    uint16_t numAsks     = read_le<uint16_t>(msg, bids_end + 2);
    size_t   asks_end    = bids_end + 4 + static_cast<size_t>(askBlockLen) * numAsks;

    if (asks_end >= msg.size()) return 0;
    uint8_t symLen = msg[asks_end];
    if (asks_end + 1 + symLen > msg.size()) return 0;

    std::string_view symbol(reinterpret_cast<const char *>(msg.data() + asks_end + 1), symLen);
    uint32_t instrument_id = 0;
    try {
        instrument_id = registry.lookup(std::string(venue_name), std::string(symbol));
    } catch (const std::exception &e) {
        std::cerr << "[SBEDecoder] " << e.what() << "\n";
        return 0;
    }

    // Stamp ingestion time once for all levels in this payload
    uint64_t recv_tsc = rdtsc_start();

    // --- Bid loop ---
    offset += 4;
    for (uint16_t i = 0; i < numBids; ++i) {
        if (offset + bidBlockLen > msg.size()) break;

        int64_t price = read_scaled(msg, offset + schema.price_field.offset, schema.price_field.raw_type);
        int64_t qty   = read_scaled(msg, offset + schema.qty_field.offset,   schema.qty_field.raw_type);

        auto *slot = queue.getNextToWriteTo();
        *slot = MarketUpdate(instrument_id, Side::BUY, price, qty, price_exp, qty_exp, event_time_us);
        slot->_recv_tsc = recv_tsc;
        queue.updateWriteIndex();

        offset += bidBlockLen;
    }

    // --- Ask loop ---
    offset += 4;
    for (uint16_t i = 0; i < numAsks; ++i) {
        if (offset + askBlockLen > msg.size()) break;

        int64_t price = read_scaled(msg, offset + schema.price_field.offset, schema.price_field.raw_type);
        int64_t qty   = read_scaled(msg, offset + schema.qty_field.offset,   schema.qty_field.raw_type);

        auto *slot = queue.getNextToWriteTo();
        *slot = MarketUpdate(instrument_id, Side::SELL, price, qty, price_exp, qty_exp, event_time_us);
        slot->_recv_tsc = recv_tsc;
        queue.updateWriteIndex();

        offset += askBlockLen;
    }

    return asks_end + 1 + symLen; // total bytes consumed
}
