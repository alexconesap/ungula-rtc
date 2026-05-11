// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include <gtest/gtest.h>

#include <cstdint>

#include <ungula/core/time/time_control.h>
#include <ungula/rtc/drivers/rtc_fake.h>
#include <ungula/rtc/rtc_time_provider.h>

namespace
{

    namespace tc = ungula::core::time;
    using ungula::rtc::RtcTimeProvider;
    using ungula::rtc::drivers::RtcFake;

    class RtcTimeProviderTest : public ::testing::Test {
    protected:
        void TearDown() override
        {
            tc::clearTimeProvider();
        }
    };

    // The host millis() runs forward by real wall time; we don't try to
    // mock it. Tests that assert on cache TTL therefore use millisecond-
    // scale TTLs so the elapsed real time stays well inside the window.

    TEST_F(RtcTimeProviderTest, IsValidFalseUntilChipInitialised)
    {
        RtcFake fake;
        RtcTimeProvider provider(fake);
        // Fresh fake: begin() not called → readEpochMs fails →
        // provider reports invalid.
        EXPECT_FALSE(provider.isValid());
    }

    TEST_F(RtcTimeProviderTest, NowMsReturnsChipEpochAfterFirstRead)
    {
        RtcFake fake;
        fake.begin(0);
        fake.setEpochMs(1'700'000'000'000LL);
        fake.setTimeValid(true);

        RtcTimeProvider provider(fake);

        const int64_t reported = provider.nowMs();
        // The provider anchors on the chip read and adds elapsed local
        // millis(). On a fast host that's ≤ a few ms — accept a small
        // window so the test stays stable.
        EXPECT_GE(reported, 1'700'000'000'000LL);
        EXPECT_LT(reported, 1'700'000'000'000LL + 1000);
        EXPECT_TRUE(provider.isValid());
    }

    TEST_F(RtcTimeProviderTest, CacheHitDoesNotReReadTheChip)
    {
        RtcFake fake;
        fake.begin(0);
        fake.setEpochMs(1'700'000'000'000LL);
        fake.setTimeValid(true);

        RtcTimeProvider provider(fake);
        provider.setRefreshIntervalMs(60'000); // 60 s window — no expiry

        (void)provider.nowMs();
        const uint32_t firstReadCount = fake.readEpochCallCount();

        // Three more calls — all should be cache hits.
        for (int i = 0; i < 3; ++i) {
            (void)provider.nowMs();
        }
        EXPECT_EQ(fake.readEpochCallCount(), firstReadCount);
    }

    TEST_F(RtcTimeProviderTest, ZeroTtlForcesEveryCallToReReadTheChip)
    {
        RtcFake fake;
        fake.begin(0);
        fake.setEpochMs(1'700'000'000'000LL);
        fake.setTimeValid(true);

        RtcTimeProvider provider(fake);
        provider.setRefreshIntervalMs(0); // disable cache

        for (int i = 0; i < 3; ++i) {
            (void)provider.nowMs();
        }
        EXPECT_GE(fake.readEpochCallCount(), 3U);
    }

    TEST_F(RtcTimeProviderTest, InvalidateCacheForcesNextReadFromChip)
    {
        RtcFake fake;
        fake.begin(0);
        fake.setEpochMs(1'000'000LL);
        fake.setTimeValid(true);

        RtcTimeProvider provider(fake);
        provider.setRefreshIntervalMs(60'000);

        (void)provider.nowMs();
        const uint32_t before = fake.readEpochCallCount();

        // Simulate an explicit set: the host writes the chip and asks
        // the provider to drop its cache so subsequent now() reflects
        // the new value.
        fake.setEpochMs(2'000'000LL);
        provider.invalidateCache();

        const int64_t reported = provider.nowMs();
        EXPECT_GE(reported, 2'000'000LL);
        EXPECT_LT(reported, 2'000'000LL + 1000);
        EXPECT_GT(fake.readEpochCallCount(), before);
    }

    TEST_F(RtcTimeProviderTest, TimeNotValidOnChipMakesProviderInvalid)
    {
        RtcFake fake;
        fake.begin(0);
        fake.setTimeValid(false); // OSF / CH set
        fake.setEpochMs(1'700'000'000'000LL); // even with a value present

        RtcTimeProvider provider(fake);
        EXPECT_FALSE(provider.isValid());
        // ITimeProvider contract says now() may return anything when
        // !isValid(); core's TimeControl::now() falls back to local
        // millis() in that case. We just verify isValid() is the gate.
    }

    TEST_F(RtcTimeProviderTest, ReadFailureMakesProviderInvalid)
    {
        RtcFake fake;
        fake.begin(0);
        fake.setTimeValid(true);
        fake.setReadResult(false); // wire is dead

        RtcTimeProvider provider(fake);
        EXPECT_FALSE(provider.isValid());
    }

    // ---- Plug-in into core's setTimeProvider ----

    TEST_F(RtcTimeProviderTest, InstallsAsCoreTimeProvider)
    {
        RtcFake fake;
        fake.begin(0);
        fake.setEpochMs(1'700'000'000'000LL);
        fake.setTimeValid(true);

        RtcTimeProvider provider(fake);
        tc::setTimeProvider(&provider);

        // tc::now() routes through the provider; provider routes through
        // the chip. Both layers must be wired correctly for this to
        // return the scripted epoch.
        const int64_t reported = tc::now();
        EXPECT_GE(reported, 1'700'000'000'000LL);
        EXPECT_LT(reported, 1'700'000'000'000LL + 1000);
    }

    TEST_F(RtcTimeProviderTest, CoreNowFallsBackToLocalWhenChipInvalid)
    {
        RtcFake fake;
        fake.begin(0);
        fake.setTimeValid(false);

        RtcTimeProvider provider(fake);
        tc::setTimeProvider(&provider);

        // Provider reports invalid → core falls back to monotonic
        // millis(), which is tiny compared to a 2023 epoch.
        EXPECT_LT(tc::now(), 1'000'000'000LL);
    }

} // namespace
