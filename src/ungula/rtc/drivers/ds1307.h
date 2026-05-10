// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <stdint.h>

#include "ungula/hal/i2c/i2c_master.h"
#include "ungula/rtc/i_rtc.h"

/// @brief Maxim DS1307 I2C real-time clock driver.
///
/// Same register layout as the DS3231 (registers 0x00..0x06 hold BCD
/// time), same I2C address (0x68). Differences from DS3231:
///   - No oscillator-stop flag. The "is time valid?" signal is the
///     **CH (clock-halt) bit** at register 0x00 bit 7. The chip ships
///     with CH set; it must be cleared once for the oscillator to start.
///   - No temperature compensation; drift is on the order of seconds
///     per day. Acceptable for most "what day is today" applications.
///
/// `writeEpochMs()` clears CH as part of writing the seconds register —
/// no separate step. `isTimeValid()` returns true iff CH is clear.

namespace ungula::rtc::drivers
{

    constexpr uint8_t DS1307_DEFAULT_ADDRESS = 0x68;

    class Ds1307 final : public IRtc {
    public:
        Ds1307(ungula::hal::i2c::I2cMaster &bus, ungula::hal::multiplexer::IMultiplexer *multiplexer = nullptr,
               const char *name = "main")
                : IRtc("DS1307", name, multiplexer)
                , bus_(bus)
        {
        }

        bool begin(uint8_t multiplexerChannel = 0) override;
        bool isConnected() override;
        bool isTimeValid() override;
        bool readEpochMs(epoch_ms_t &out) override;
        bool writeEpochMs(epoch_ms_t epochMs) override;

    private:
        bool readSecondsRegister(uint8_t &out);

        ungula::hal::i2c::I2cMaster &bus_;
    };

} // namespace ungula::rtc::drivers
