// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <stdint.h>

#include "ungula/hal/i2c/i2c_master.h"
#include "ungula/rtc/i_rtc.h"

/// @brief Maxim DS3231 high-precision I2C real-time clock driver.
///
/// TCXO-compensated, ±2 ppm typical. Time-keeping registers at 0x00..0x06
/// (BCD: seconds, minutes, hours, day-of-week, date, month, year). Status
/// register 0x0F holds the OSF (oscillator-stop flag) bit 7 — set by the
/// chip whenever the oscillator has stopped, including the very first
/// power-up before the battery has run. Cleared on a successful
/// `writeEpochMs()`.
///
/// I2C address is fixed at `0x68`. Default 400 kHz works on every part.

namespace ungula::rtc::drivers
{

constexpr uint8_t DS3231_DEFAULT_ADDRESS = 0x68;

class Ds3231 final : public IRtc {
    public:
        /// @param bus         I2C bus the chip lives on. Borrowed.
        /// @param multiplexer Optional. `nullptr` means direct-connect.
        /// @param name        Caller-chosen tag, e.g. "main". Borrowed.
        Ds3231(ungula::hal::i2c::I2cMaster &bus,
               ungula::hal::multiplexer::IMultiplexer *multiplexer = nullptr,
               const char *name = "main")
                : IRtc("DS3231", name, multiplexer)
                , bus_(bus)
        {
        }

        bool begin(uint8_t multiplexerChannel = 0) override;
        bool isConnected() override;
        bool isTimeValid() override;
        bool readEpochMs(epoch_ms_t &out) override;
        bool writeEpochMs(epoch_ms_t epochMs) override;

    private:
        bool readStatusRegister(uint8_t &out);
        bool writeStatusRegister(uint8_t value);

        ungula::hal::i2c::I2cMaster &bus_;
};

} // namespace ungula::rtc::drivers
