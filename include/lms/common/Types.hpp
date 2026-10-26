#pragma once
// Basic identifier and time types shared by every layer.
#include <cstdint>

namespace lms {

using Id = std::int64_t;         // database row identifier (0 = "not saved yet")
using Timestamp = std::int64_t;  // seconds since 1970-01-01 00:00:00 UTC

inline constexpr Id kNoId = 0;
inline constexpr Timestamp kSecondsPerDay = 86400;

}  // namespace lms
