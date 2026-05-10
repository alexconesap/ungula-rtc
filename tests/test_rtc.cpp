// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

#include <ungula/hal/multiplexer/drivers/multiplexer_fake.h>
#include <ungula/rtc/drivers/rtc_fake.h>
#include <ungula/rtc/i_rtc.h>

namespace
{

    using ungula::hal::multiplexer::drivers::MultiplexerFake;
    using ungula::rtc::Error;
    using ungula::rtc::IRtc;
    using ungula::rtc::drivers::RtcFake;

    // ---- Interface contract / drift detection ------------------------------

    TEST(IRtcContract, FakeImplementsEveryPureVirtual)
    {
        RtcFake fake;
        IRtc *api = static_cast<IRtc *>(&fake);
        EXPECT_TRUE(api->begin(0));
        EXPECT_TRUE(api->isConnected());
        EXPECT_TRUE(api->writeEpochMs(1 '700' 000 '000' 000LL));
        EXPECT_TRUE(api->isTimeValid());
        ungula::rtc::epoch_ms_t now = 0;
        EXPECT_TRUE(api->readEpochMs(now));
        EXPECT_EQ(now, 1 '700' 000 '000' 000LL);
    }

    // ---- Multiplexer optional ---------------------------------------------

    TEST(IRtc, DirectConnectHasNoMultiplexer)
    {
        RtcFake fake;
        EXPECT_FALSE(fake.hasMultiplexer());
    }

    TEST(IRtc, ReadFailsWhenBeginNeverCalled)
    {
        RtcFake fake;
        ungula::rtc::epoch_ms_t out = 0;
        EXPECT_FALSE(fake.readEpochMs(out));
        EXPECT_EQ(fake.getLastError(), Error::NotInitialized);
    }

    TEST(IRtc, MultiplexedReadSelectsTheConfiguredChannel)
    {
        MultiplexerFake mux;
        mux.begin();

        RtcFake fake("main", &mux);
        fake.begin(/*channel=*/2);
        fake.setEpochMs(42'000LL);
        fake.setTimeValid(true);

        ungula::rtc::epoch_ms_t now = 0;
        EXPECT_TRUE(fake.readEpochMs(now));
        EXPECT_EQ(now, 42'000LL);
        EXPECT_EQ(mux.lastChannel(), 2U);
    }

    TEST(IRtc, MultiplexerFailurePropagatesAsError)
    {
        MultiplexerFake mux;
        mux.begin();
        mux.setSelectAlwaysFails(true);

        RtcFake fake("main", &mux);
        fake.begin(2);

        ungula::rtc::epoch_ms_t out = 0;
        EXPECT_FALSE(fake.readEpochMs(out));
        EXPECT_EQ(fake.getLastError(), Error::MultiplexerError);
    }

    // ---- Validity flag ----------------------------------------------------

    TEST(IRtc, FreshChipReportsTimeNotValid)
    {
        RtcFake fake;
        fake.begin(0);
        // The fake mirrors a freshly-powered chip with battery never set:
        // isTimeValid() must return false until a successful write.
        EXPECT_FALSE(fake.isTimeValid());
    }

    TEST(IRtc, SuccessfulWriteFlipsValidityToTrue)
    {
        RtcFake fake;
        fake.begin(0);
        EXPECT_FALSE(fake.isTimeValid());
        EXPECT_TRUE(fake.writeEpochMs(1 '700' 000 '000' 000LL));
        EXPECT_TRUE(fake.isTimeValid());
    }

    TEST(IRtc, FailedWriteLeavesValidityUntouched)
    {
        RtcFake fake;
        fake.begin(0);
        fake.setWriteResult(false);
        EXPECT_FALSE(fake.writeEpochMs(1'234LL));
        EXPECT_EQ(fake.getLastError(), Error::I2CWriteError);
        EXPECT_FALSE(fake.isTimeValid());
    }

    TEST(IRtc, ReadFailureSurfacesAsI2CReadError)
    {
        RtcFake fake;
        fake.begin(0);
        fake.setReadResult(false);
        ungula::rtc::epoch_ms_t out = 0;
        EXPECT_FALSE(fake.readEpochMs(out));
        EXPECT_EQ(fake.getLastError(), Error::I2CReadError);
    }

    // ---- Logging toggle ---------------------------------------------------

    TEST(IRtcLogging, DefaultsOff)
    {
        RtcFake fake;
        EXPECT_FALSE(fake.isLoggingEnabled());
    }

    TEST(IRtcLogging, EnableDisableFlipsFlag)
    {
        RtcFake fake;
        fake.enableLogging();
        EXPECT_TRUE(fake.isLoggingEnabled());
        fake.disableLogging();
        EXPECT_FALSE(fake.isLoggingEnabled());
    }

    // ---- Error mapping ----------------------------------------------------

    TEST(IRtc, GetLastErrorAsStrCoversEveryEnumValue)
    {
        RtcFake fake;
        const Error allValues[] = {
            Error::None,         Error::NotInitialized,   Error::BeginFailed,
            Error::NotConnected, Error::MultiplexerError, Error::TimeNotValid,
            Error::I2CReadError, Error::I2CWriteError,
        };
        for (Error e : allValues) {
            fake.setStatus(e);
            const char *msg = fake.getLastErrorAsStr();
            ASSERT_NE(msg, nullptr);
            EXPECT_GT(strlen(msg), 0U) << "missing message for enum " << static_cast<int>(e);
        }
    }

} // namespace
