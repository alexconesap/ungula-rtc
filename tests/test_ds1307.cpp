// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include <gtest/gtest.h>

#include <ungula/hal/i2c/i2c_master.h>
#include <ungula/rtc/drivers/ds1307.h>
#include <ungula/rtc/i_rtc.h>

namespace {

    using ungula::hal::i2c::I2cMaster;
    using ungula::rtc::IRtc;
    using ungula::rtc::drivers::DS1307_DEFAULT_ADDRESS;
    using ungula::rtc::drivers::Ds1307;

    TEST(Ds1307, AddressConstantIsTheChipDefault) {
        EXPECT_EQ(static_cast<uint8_t>(DS1307_DEFAULT_ADDRESS), 0x68);
    }

    TEST(Ds1307, IsAValidIRtc) {
        I2cMaster bus(0);
        Ds1307 chip(bus);
        IRtc* api = static_cast<IRtc*>(&chip);
        EXPECT_NE(api, nullptr);
        EXPECT_STREQ(chip.getModel(), "DS1307");
    }

    TEST(Ds1307, BeginOnUnreachableBusFlagsBeginFailed) {
        I2cMaster bus(0);
        Ds1307 chip(bus);
        EXPECT_FALSE(chip.begin());
        EXPECT_EQ(chip.getLastError(), ungula::rtc::Error::BeginFailed);
    }

    TEST(Ds1307, ReadEpochWithoutBeginReturnsFalse) {
        I2cMaster bus(0);
        Ds1307 chip(bus);
        ungula::rtc::epoch_ms_t out = 999;
        EXPECT_FALSE(chip.readEpochMs(out));
        EXPECT_EQ(chip.getLastError(), ungula::rtc::Error::NotInitialized);
    }

}  // namespace
