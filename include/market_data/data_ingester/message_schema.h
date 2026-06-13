#pragma once

#include <cstdint>
#include <cstddef>

enum class IntEncoding  { INT8, INT16, INT32, INT64, UINT8, UINT16, UINT32, UINT64 };
enum class TimestampUnit { MICROSECONDS, MILLISECONDS, NANOSECONDS };
enum class SideSource    { IMPLICIT_BY_GROUP, INLINE_FIELD };

struct ScaledField
{
    size_t      offset;    // byte offset within a level block
    IntEncoding raw_type;
};

struct SideFieldDef
{
    SideSource  source;
    size_t      offset     = 0;   // byte offset within level block (INLINE_FIELD only)
    IntEncoding raw_type   = IntEncoding::UINT8;
    uint64_t    buy_value  = 0;   // raw value meaning BUY (INLINE_FIELD only)
};

struct MessageSchema
{
    uint16_t      template_id;
    uint16_t      schema_id;

    // Fixed block layout (immediately after the 8-byte SBE header)
    size_t        fixed_block_size;
    size_t        event_time_offset;   // within fixed block
    TimestampUnit event_time_unit;

    // Shared exponent fields within the fixed block
    size_t        price_exp_offset;
    size_t        qty_exp_offset;

    // Per-level field descriptors
    ScaledField   price_field;
    ScaledField   qty_field;
    SideFieldDef  side_field;
};

// Binance SBE depth-update schema (templateId=1, schemaId=1)
inline constexpr MessageSchema kBinanceDepthV1 {
    .template_id       = 1,
    .schema_id         = 1,
    .fixed_block_size  = 26,
    .event_time_offset = 0,
    .event_time_unit   = TimestampUnit::MILLISECONDS,
    .price_exp_offset  = 24,
    .qty_exp_offset    = 25,
    .price_field       = { 0, IntEncoding::INT64 },
    .qty_field         = { 8, IntEncoding::INT64 },
    .side_field        = { SideSource::IMPLICIT_BY_GROUP },
};
