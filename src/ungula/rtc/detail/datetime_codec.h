// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <stdint.h>
#include <time.h>

#include "bcd.h"

/// @brief Convert between `epoch_ms_t` and the broken-down Y/M/D h:m:s
/// tuple that DS1307/DS3231 expose in registers 0x00..0x06. UTC by
/// convention — same convention the rest of `ungula::core::time` uses.
///
/// Implementation notes:
///   - Years on the chip are 0..99, biased to 2000. Year 2100 wraps;
///     the project does not target devices that need to outlive that.
///   - We use `gmtime_r` for decode (epoch -> tuple) and a hand-rolled
///     inverse for encode so we don't depend on `timegm` (non-POSIX) or
///     touch the system TZ.

namespace ungula::rtc::detail
{

/// Broken-down time as it sits on the wire.
struct DateTime {
        uint16_t year; // 2000..2099
        uint8_t month; // 1..12
        uint8_t day; // 1..31
        uint8_t hour; // 0..23
        uint8_t minute; // 0..59
        uint8_t second; // 0..59
        uint8_t dayOfWeek; // 1..7, chip convention
};

/// Days from 1970-01-01 to the start of the given year (inclusive of
/// leap days). Pure arithmetic — no system calls.
constexpr int32_t daysFromEpochToYear(uint16_t year)
{
        // year is at least 1970 in any sane caller path. For safety we
        // return 0 for anything before that.
        if (year < 1970U) {
                return 0;
        }
        const int32_t y = static_cast<int32_t>(year);
        // Howard Hinnant's date library has a much faster closed form
        // but this loop is bounded to ~130 iterations — fine for boot
        // and for the once-per-write-time path.
        int32_t days = 0;
        for (int32_t i = 1970; i < y; ++i) {
                const bool leap = (i % 4 == 0 && i % 100 != 0) || (i % 400 == 0);
                days += leap ? 366 : 365;
        }
        return days;
}

constexpr bool isLeapYear(uint16_t y)
{
        return (y % 4U == 0U && y % 100U != 0U) || (y % 400U == 0U);
}

constexpr uint8_t daysInMonth(uint16_t y, uint8_t m)
{
        constexpr uint8_t LUT[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        if (m < 1U || m > 12U) {
                return 0U;
        }
        if (m == 2U && isLeapYear(y)) {
                return 29U;
        }
        return LUT[m - 1U];
}

/// Encode a `DateTime` (UTC) into Unix epoch milliseconds.
/// Returns 0 if any field is out of range.
inline int64_t toEpochMs(const DateTime &dt)
{
        if (dt.year < 1970U || dt.month < 1U || dt.month > 12U || dt.day < 1U ||
            dt.day > daysInMonth(dt.year, dt.month) || dt.hour > 23U || dt.minute > 59U ||
            dt.second > 59U) {
                return 0;
        }
        int32_t days = daysFromEpochToYear(dt.year);
        for (uint8_t m = 1U; m < dt.month; ++m) {
                days += daysInMonth(dt.year, m);
        }
        days += static_cast<int32_t>(dt.day) - 1;

        const int64_t seconds =
            static_cast<int64_t>(days) * 86400 + static_cast<int64_t>(dt.hour) * 3600 +
            static_cast<int64_t>(dt.minute) * 60 + static_cast<int64_t>(dt.second);
        return seconds * 1000;
}

/// Decode Unix epoch milliseconds into a `DateTime` (UTC).
/// Negative or zero inputs are clamped to 1970-01-01 00:00:00.
inline DateTime fromEpochMs(int64_t epochMs)
{
        DateTime dt{};
        if (epochMs < 0) {
                epochMs = 0;
        }
        const time_t epochSeconds = static_cast<time_t>(epochMs / 1000);
        struct tm timeinfo{};
        gmtime_r(&epochSeconds, &timeinfo);
        dt.year = static_cast<uint16_t>(timeinfo.tm_year + 1900);
        dt.month = static_cast<uint8_t>(timeinfo.tm_mon + 1);
        dt.day = static_cast<uint8_t>(timeinfo.tm_mday);
        dt.hour = static_cast<uint8_t>(timeinfo.tm_hour);
        dt.minute = static_cast<uint8_t>(timeinfo.tm_min);
        dt.second = static_cast<uint8_t>(timeinfo.tm_sec);
        // tm_wday is 0..6 (Sunday=0). The chips use 1..7 with the start
        // day being host policy — we adopt 1=Sunday so the round-trip is
        // stable. Hosts that care about the exact mapping should re-set it.
        dt.dayOfWeek = static_cast<uint8_t>(timeinfo.tm_wday + 1);
        return dt;
}

} // namespace ungula::rtc::detail
