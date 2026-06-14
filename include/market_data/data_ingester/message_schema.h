#pragma once

#include <cstdint>
#include <cstddef>

enum class IntEncoding   { INT8, INT16, INT32, INT64, UINT8, UINT16, UINT32, UINT64 };
enum class TimestampUnit { MICROSECONDS, MILLISECONDS, NANOSECONDS };
enum class SideSource    { IMPLICIT_BY_GROUP, INLINE_FIELD };
enum class GroupNumEncoding { UINT16, UINT32 };  // groupSize16Encoding vs groupSizeEncoding

struct ScaledField
{
    size_t      offset;    // byte offset within a level block
    IntEncoding raw_type;
};

struct SideFieldDef
{
    SideSource  source;
    size_t      offset     = 0;
    IntEncoding raw_type   = IntEncoding::UINT8;
    uint64_t    buy_value  = 0;
};

struct MessageSchema
{
    uint16_t      template_id;
    uint16_t      schema_id;

    size_t        fixed_block_size;
    size_t        event_time_offset;
    TimestampUnit event_time_unit;

    size_t        price_exp_offset;
    size_t        qty_exp_offset;

    ScaledField   price_field;
    ScaledField   qty_field;
    SideFieldDef  side_field;

    // Group structure metadata — used by measure() and skip path
    uint8_t          num_groups         = 2;
    GroupNumEncoding group_num_encoding = GroupNumEncoding::UINT16;
    uint8_t          num_vardata        = 1;  // trailing varString8 fields (symbol)
    bool             skip               = false;  // advance cursor without enqueuing
};

// DepthDiffStreamEvent (id=10003) — btcusdt@depth, ethusdt@depth, bnbusdt@depth
// Fixed block: eventTime(8) + firstUpdateId(8) + lastUpdateId(8) + priceExp(1) + qtyExp(1) = 26
inline constexpr MessageSchema kBinanceDepthDiff {
    .template_id       = 10003,
    .schema_id         = 1,
    .fixed_block_size  = 26,
    .event_time_offset = 0,
    .event_time_unit   = TimestampUnit::MICROSECONDS,
    .price_exp_offset  = 24,
    .qty_exp_offset    = 25,
    .price_field       = { 0, IntEncoding::INT64 },
    .qty_field         = { 8, IntEncoding::INT64 },
    .side_field        = { SideSource::IMPLICIT_BY_GROUP },
    .num_groups        = 2,
    .group_num_encoding = GroupNumEncoding::UINT16,
    .num_vardata       = 1,
};

// DepthSnapshotStreamEvent (id=10002) — btcusdt@depth5/10/20
// Fixed block: eventTime(8) + bookUpdateId(8) + priceExp(1) + qtyExp(1) = 18
inline constexpr MessageSchema kBinanceDepthSnapshot {
    .template_id       = 10002,
    .schema_id         = 1,
    .fixed_block_size  = 18,
    .event_time_offset = 0,
    .event_time_unit   = TimestampUnit::MICROSECONDS,
    .price_exp_offset  = 16,
    .qty_exp_offset    = 17,
    .price_field       = { 0, IntEncoding::INT64 },
    .qty_field         = { 8, IntEncoding::INT64 },
    .side_field        = { SideSource::IMPLICIT_BY_GROUP },
    .num_groups        = 2,
    .group_num_encoding = GroupNumEncoding::UINT16,
    .num_vardata       = 1,
};

// BestBidAskStreamEvent (id=10001) — bookTicker@<sym>
// Fixed block: 50 bytes (no repeating groups; bid/ask inline). Registered as skip=true
// because the current decoder path expects two depth groups.
inline constexpr MessageSchema kBinanceBestBidAsk {
    .template_id       = 10001,
    .schema_id         = 1,
    .fixed_block_size  = 50,
    .event_time_offset = 0,
    .event_time_unit   = TimestampUnit::MICROSECONDS,
    .price_exp_offset  = 16,
    .qty_exp_offset    = 17,
    .price_field       = { 0, IntEncoding::INT64 },
    .qty_field         = { 8, IntEncoding::INT64 },
    .side_field        = { SideSource::IMPLICIT_BY_GROUP },
    .num_groups        = 0,
    .group_num_encoding = GroupNumEncoding::UINT16,
    .num_vardata       = 1,
    .skip              = true,
};

// TradesStreamEvent (id=10000) — trade@<sym>
// Fixed block: 18 bytes; ONE group using groupSizeEncoding (uint32 numInGroup). Registered
// as skip=true; trades are not used for order-book updates.
inline constexpr MessageSchema kBinanceTrades {
    .template_id       = 10000,
    .schema_id         = 1,
    .fixed_block_size  = 18,
    .event_time_offset = 0,
    .event_time_unit   = TimestampUnit::MICROSECONDS,
    .price_exp_offset  = 16,
    .qty_exp_offset    = 17,
    .price_field       = { 8,  IntEncoding::INT64 },  // offset past 8-byte id field
    .qty_field         = { 16, IntEncoding::INT64 },
    .side_field        = { SideSource::IMPLICIT_BY_GROUP },
    .num_groups        = 1,
    .group_num_encoding = GroupNumEncoding::UINT32,
    .num_vardata       = 1,
    .skip              = true,
};
