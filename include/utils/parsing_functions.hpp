#pragma once
#include <cstdint>
#include <span>

inline int64_t read_int64_le(std::span<const uint8_t> &payload, size_t offset)
{
    if (offset + sizeof(int64_t) > payload.size())
        return 0;

    int64_t value;
    std::memcpy(&value, payload.data() + offset, sizeof(int64_t));
    return value;
}

inline uint16_t read_uint16_le(std::span<const uint8_t> &payload, size_t offset)
{
    if (offset + sizeof(uint16_t) > payload.size())
        return 0;

    uint16_t value;
    std::memcpy(&value, payload.data() + offset, sizeof(uint16_t));
    return value;
}

inline int8_t read_int8(std::span<const uint8_t> &payload, size_t offset)
{
    if (offset + sizeof(int8_t) > payload.size())
        return 0;

    uint16_t value;
    std::memcpy(&value, payload.data() + offset, sizeof(int8_t));
    return value;
}

// auto read_int8 = [&](size_t offset) -> int8_t
// {
//     if (offset >= payload.size())
//         return 0;
//     int8_t value;
//     std::memcpy(&value, &payload[offset], sizeof(int8_t));
//     return value;
// };

// auto read_uint8 = [&](size_t offset) -> uint8_t
// {
//     if (offset >= payload.size())
//         return 0;
//     uint8_t value;
//     std::memcpy(&value, &payload[offset], sizeof(uint8_t));
//     return value;
// };
// auto read_bool = [&](size_t offset) -> bool
// {
//     if (offset >= payload.size())
//         return false;
//     return payload[offset] != 0;
// };
