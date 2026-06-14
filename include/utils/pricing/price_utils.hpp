#pragma once

#include <cstdint>
#include <cmath>
#include <cstdio>
#include <string>

namespace PriceUtils {

// For logging, display, and PCModel output only — not on the hot path.
inline double to_double(int64_t raw, int8_t exp) noexcept
{
    return static_cast<double>(raw) * std::pow(10.0, exp);
}

inline std::string to_string(int64_t raw, int8_t exp)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.8g", to_double(raw, exp));
    return std::string(buf);
}

} // namespace PriceUtils
