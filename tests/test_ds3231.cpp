// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include <gtest/gtest.h>

#include <ungula/hal/i2c/i2c_master.h>
#include <ungula/rtc/drivers/ds3231.h>
#include <ungula/rtc/i_rtc.h>

namespace
{

using ungula::hal::i2c::I2cMaster;
using ungula::rtc::IRtc;
using ungula::rtc::drivers::DS3231_DEFAULT_ADDRESS;
using ungula::rtc::drivers::Ds3231;

// The host I2cMaster is a no-op stub: every transfer returns false.
// These tests verify behaviour at the boundary the driver controls —
// construction, address constant, and what happens when the wire is
// unreachable. Full hardware path is exercised on the device.

TEST(Ds3231, AddressConstantIsTheChipDefault)
{
        EXPECT_EQ(static_cast<uint8_t>(DS3231_DEFAULT_ADDRESS), 0x68);
}

TEST(Ds3231, IsAValidIRtc)
{
        I2cMaster bus(0);
        Ds3231 chip(bus);
        IRtc *api = static_cast<IRtc *>(&chip);
        EXPECT_NE(api, nullptr);
        EXPECT_STREQ(chip.getModel(), "DS3231");
}

TEST(Ds3231, BeginOnUnreachableBusFlagsBeginFailed)
{
        I2cMaster bus(0);
        Ds3231 chip(bus);
        EXPECT_FALSE(chip.begin());
        EXPECT_EQ(chip.getLastError(), ungula::rtc::Error::BeginFailed);
        EXPECT_EQ(chip.getAddress(), 0x68);
}

TEST(Ds3231, ReadEpochWithoutBeginReturnsFalse)
{
        I2cMaster bus(0);
        Ds3231 chip(bus);
        ungula::rtc::epoch_ms_t out = 12345;
        EXPECT_FALSE(chip.readEpochMs(out));
        EXPECT_EQ(chip.getLastError(), ungula::rtc::Error::NotInitialized);
}

TEST(Ds3231, IsTimeValidWithoutBeginReturnsFalse)
{
        I2cMaster bus(0);
        Ds3231 chip(bus);
        EXPECT_FALSE(chip.isTimeValid());
}

} // namespace
