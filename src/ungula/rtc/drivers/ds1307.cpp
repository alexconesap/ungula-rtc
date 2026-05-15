// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include "ds1307.h"

#include "ungula/rtc/detail/bcd.h"
#include "ungula/rtc/detail/datetime_codec.h"

namespace ungula::rtc::drivers
{

namespace
{
        constexpr uint8_t REG_TIME_BASE = 0x00;
        constexpr uint8_t SECONDS_CH_MASK = 0x80; // bit 7 of seconds reg = clock halt
        constexpr uint8_t HOUR_24H_MASK = 0x3F;
} // namespace

using detail::bcdToBin;
using detail::binToBcd;

bool Ds1307::begin(uint8_t multiplexerChannel)
{
        multiplexerChannel_ = multiplexerChannel;
        address_ = DS1307_DEFAULT_ADDRESS;
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

bool Ds1307::isConnected()
{
        return bus_.write(address_, nullptr, 0);
}

bool Ds1307::readSecondsRegister(uint8_t &out)
{
        const uint8_t reg = REG_TIME_BASE;
        if (!bus_.writeRead(address_, &reg, 1, &out, 1)) {
                last_error_ = Error::I2CReadError;
                return false;
        }
        return true;
}

bool Ds1307::isTimeValid()
{
        if (!selectMultiplexerChannel()) {
                return false;
        }
        uint8_t seconds = 0;
        if (!readSecondsRegister(seconds)) {
                logErrorf("seconds register read failed");
                return false;
        }
        // CH == 1 means the clock is halted — i.e. the chip has never
        // been properly started, or has been deliberately stopped.
        return (seconds & SECONDS_CH_MASK) == 0;
}

bool Ds1307::readEpochMs(epoch_ms_t &out)
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
        // Mask the CH bit before decoding seconds — the chip leaves it
        // in the BCD register but it must not be interpreted as data.
        dt.second = bcdToBin(buf[0] & 0x7FU);
        dt.minute = bcdToBin(buf[1] & 0x7FU);
        dt.hour = bcdToBin(buf[2] & HOUR_24H_MASK);
        dt.dayOfWeek = bcdToBin(buf[3] & 0x07U);
        dt.day = bcdToBin(buf[4] & 0x3FU);
        dt.month = bcdToBin(buf[5] & 0x1FU);
        dt.year = static_cast<uint16_t>(2000U + bcdToBin(buf[6]));

        out = detail::toEpochMs(dt);
        clearLastError();
        return true;
}

bool Ds1307::writeEpochMs(epoch_ms_t epochMs)
{
        if (!selectMultiplexerChannel()) {
                return false;
        }
        const detail::DateTime dt = detail::fromEpochMs(epochMs);
        if (dt.year < 2000U || dt.year > 2099U) {
                last_error_ = Error::I2CWriteError;
                logErrorf("year %u out of DS1307 range (2000..2099)", dt.year);
                return false;
        }

        // Writing 0 into bit 7 of the seconds register clears CH, so the
        // single block-write also re-starts the oscillator if it was halted.
        const uint8_t buf[8] = {
                REG_TIME_BASE,
                binToBcd(dt.second), // CH cleared (bit 7 = 0)
                binToBcd(dt.minute),
                binToBcd(dt.hour), // 24-hour mode (bit 6 = 0)
                binToBcd(dt.dayOfWeek), binToBcd(dt.day),
                binToBcd(dt.month),     binToBcd(static_cast<uint8_t>(dt.year - 2000U)),
        };
        if (!bus_.write(address_, buf, sizeof(buf))) {
                last_error_ = Error::I2CWriteError;
                logErrorf("time write failed");
                return false;
        }

        clearLastError();
        logInfof("time set");
        return true;
}

} // namespace ungula::rtc::drivers
