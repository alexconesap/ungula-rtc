// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <stdint.h>

/// @brief BCD <-> binary helpers used by every RTC driver.
///
/// The DS1307 and DS3231 (and most I2C RTCs) store time as packed BCD —
/// upper nibble = tens digit, lower nibble = units digit. Two values per
/// byte. Range here is 0..99; out-of-range inputs return 0 (the chips
/// can't produce them anyway).

namespace ungula::rtc::detail
{

    constexpr uint8_t bcdToBin(uint8_t bcd)
    {
        return static_cast<uint8_t>(((bcd >> 4) & 0x0FU) * 10U + (bcd & 0x0FU));
    }

    constexpr uint8_t binToBcd(uint8_t bin)
    {
        if (bin > 99U) {
            return 0U;
        }
        return static_cast<uint8_t>(((bin / 10U) << 4) | (bin % 10U));
    }

} // namespace ungula::rtc::detail
