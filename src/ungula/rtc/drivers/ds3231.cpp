// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include "ds3231.h"

#include "ungula/rtc/detail/bcd.h"
#include "ungula/rtc/detail/datetime_codec.h"

namespace ungula::rtc::drivers
{

    namespace
    {
        constexpr uint8_t REG_TIME_BASE = 0x00;
        constexpr uint8_t REG_STATUS = 0x0F;
        constexpr uint8_t STATUS_OSF_MASK = 0x80; // bit 7 = oscillator stopped

        constexpr uint8_t HOUR_24H_MASK = 0x3F; // 24-hour mode: bits 0..5 hold the hour
    } // namespace

    using detail::bcdToBin;
    using detail::binToBcd;

    bool Ds3231::begin(uint8_t multiplexerChannel)
    {
        multiplexerChannel_ = multiplexerChannel;
        address_ = DS3231_DEFAULT_ADDRESS;
        // Mark initialised before the probe so that a probe failure
        // surfaces as BeginFailed rather than NotInitialized.
        isInitialized_ = true;

        if (!selectMultiplexerChannel()) {
            setInitializationStatus(Error::MultiplexerError);
            return false;
        }
        if (!isConnected()) {
            setInitializationStatus(Error::BeginFailed);
            logErrorf("not connected on begin()");
            return false;
        }

        clearLastError();
        logInfof("ready");
        return true;
    }

    bool Ds3231::isConnected()
    {
        return bus_.write(address_, nullptr, 0);
    }

    bool Ds3231::readStatusRegister(uint8_t &out)
    {
        const uint8_t reg = REG_STATUS;
        if (!bus_.writeRead(address_, &reg, 1, &out, 1)) {
            last_error_ = Error::I2CReadError;
            return false;
        }
        return true;
    }

    bool Ds3231::writeStatusRegister(uint8_t value)
    {
        const uint8_t buf[2] = { REG_STATUS, value };
        if (!bus_.write(address_, buf, sizeof(buf))) {
            last_error_ = Error::I2CWriteError;
            return false;
        }
        return true;
    }

    bool Ds3231::isTimeValid()
    {
        if (!selectMultiplexerChannel()) {
            return false;
        }
        uint8_t status = 0;
        if (!readStatusRegister(status)) {
            logErrorf("status read failed");
            return false;
        }
        // OSF == 1 → oscillator has stopped at some point since the last
        // explicit clear. Time may have drifted; treat as invalid.
        return (status & STATUS_OSF_MASK) == 0;
    }

    bool Ds3231::readEpochMs(epoch_ms_t &out)
    {
        if (!selectMultiplexerChannel()) {
            return false;
        }
        const uint8_t reg = REG_TIME_BASE;
        uint8_t buf[7] = { 0 };
        if (!bus_.writeRead(address_, &reg, 1, buf, sizeof(buf))) {
            last_error_ = Error::I2CReadError;
            logErrorf("time read failed");
            return false;
        }

        detail::DateTime dt{};
        dt.second = bcdToBin(buf[0] & 0x7FU); // bit 7 reserved on DS3231
        dt.minute = bcdToBin(buf[1] & 0x7FU);
        dt.hour = bcdToBin(buf[2] & HOUR_24H_MASK); // 24-hour mode by convention
        dt.dayOfWeek = bcdToBin(buf[3] & 0x07U);
        dt.day = bcdToBin(buf[4] & 0x3FU);
        dt.month = bcdToBin(buf[5] & 0x1FU); // bit 7 = century, ignored
        dt.year = static_cast<uint16_t>(2000U + bcdToBin(buf[6]));

        out = detail::toEpochMs(dt);
        clearLastError();
        return true;
    }

    bool Ds3231::writeEpochMs(epoch_ms_t epochMs)
    {
        if (!selectMultiplexerChannel()) {
            return false;
        }
        const detail::DateTime dt = detail::fromEpochMs(epochMs);
        if (dt.year < 2000U || dt.year > 2099U) {
            // The chip stores YY 00..99 biased to 2000. Anything outside
            // that range can't round-trip; refuse rather than truncate.
            last_error_ = Error::I2CWriteError;
            logErrorf("year %u out of DS3231 range (2000..2099)", dt.year);
            return false;
        }

        const uint8_t buf[8] = {
            REG_TIME_BASE,
            binToBcd(dt.second),
            binToBcd(dt.minute),
            binToBcd(dt.hour), // 24-hour mode
            binToBcd(dt.dayOfWeek),
            binToBcd(dt.day),
            binToBcd(dt.month), // bit 7 (century) = 0
            binToBcd(static_cast<uint8_t>(dt.year - 2000U)),
        };
        if (!bus_.write(address_, buf, sizeof(buf))) {
            last_error_ = Error::I2CWriteError;
            logErrorf("time write failed");
            return false;
        }

        // Clear the OSF bit so subsequent isTimeValid() reflects the
        // fresh write. Read-modify-write so we don't disturb the other
        // bits in the status register.
        uint8_t status = 0;
        if (!readStatusRegister(status)) {
            // Time was written; just couldn't clear OSF. Surface a write
            // error so the host knows the validity flag may still be set.
            return false;
        }
        if ((status & STATUS_OSF_MASK) != 0) {
            if (!writeStatusRegister(static_cast<uint8_t>(status & ~STATUS_OSF_MASK))) {
                logErrorf("OSF clear failed");
                return false;
            }
        }

        clearLastError();
        logInfof("time set");
        return true;
    }

} // namespace ungula::rtc::drivers
