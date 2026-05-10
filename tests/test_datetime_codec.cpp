// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include <gtest/gtest.h>

#include <cstdint>

#include <ungula/rtc/detail/datetime_codec.h>

namespace
{

    using ungula::rtc::detail::DateTime;
    using ungula::rtc::detail::fromEpochMs;
    using ungula::rtc::detail::isLeapYear;
    using ungula::rtc::detail::toEpochMs;

    TEST(DateTimeCodec, KnownReferenceInstantUtc)
    {
        // 2023-11-14 22:13:20 UTC == 1700000000 s == 1700000000000 ms.
        // Same anchor used by the time_format / TimeControl tests, so a
        // mismatch here means we drifted from the rest of the codebase.
        DateTime dt{};
        dt.year = 2023;
        dt.month = 11;
        dt.day = 14;
        dt.hour = 22;
        dt.minute = 13;
        dt.second = 20;
        EXPECT_EQ(toEpochMs(dt), 1 '700' 000 '000' 000LL);
    }

    TEST(DateTimeCodec, EpochZeroDecodesToUnixEpoch)
    {
        const DateTime dt = fromEpochMs(0);
        EXPECT_EQ(dt.year, 1970);
        EXPECT_EQ(dt.month, 1);
        EXPECT_EQ(dt.day, 1);
        EXPECT_EQ(dt.hour, 0);
        EXPECT_EQ(dt.minute, 0);
        EXPECT_EQ(dt.second, 0);
    }

    TEST(DateTimeCodec, RoundTripPreservesValuesInRtcRange)
    {
        const int64_t samples[] = {
            0,
            946 '684' 800 '000LL,   // 2000-01-01 00:00:00 — DS1307/DS3231 base 1 ' 234 ' 567 ' 890' 123LL, // arbitrary 2009 instant 1 '700' 000 '000' 000LL, // 2023-11-14 22:13:20
            4 '102' 444 '799' 000LL, // 2099-12-31 23:59:59 — top of chip range
        };
        for (int64_t ms : samples) {
            // Round-trip is at second resolution because the codec drops
            // sub-second on the way to the chip and reads back integer
            // seconds — same lossy boundary the real chip imposes.
            const int64_t secondsAligned = (ms / 1000) * 1000;
            DateTime dt = fromEpochMs(ms);
            EXPECT_EQ(toEpochMs(dt), secondsAligned) << "ms=" << ms;
        }
    }

    TEST(DateTimeCodec, OutOfRangeFieldsReturnZero)
    {
        DateTime bad{};
        bad.year = 2024;
        bad.month = 13; // invalid
        bad.day = 1;
        EXPECT_EQ(toEpochMs(bad), 0);

        bad.month = 2;
        bad.day = 30; // invalid Feb 30
        EXPECT_EQ(toEpochMs(bad), 0);

        bad.month = 2;
        bad.day = 29;
        bad.year = 2023; // 2023 is not a leap year
        EXPECT_EQ(toEpochMs(bad), 0);

        bad.year = 2024; // leap year — should now succeed
        EXPECT_NE(toEpochMs(bad), 0);
    }

    TEST(DateTimeCodec, LeapYearRulesMatchGregorian)
    {
        EXPECT_TRUE(isLeapYear(2000)); // divisible by 400
        EXPECT_FALSE(isLeapYear(1900)); // divisible by 100 but not 400
        EXPECT_TRUE(isLeapYear(2024));
        EXPECT_FALSE(isLeapYear(2023));
        EXPECT_TRUE(isLeapYear(2400));
    }

    TEST(DateTimeCodec, NegativeInputClampsToEpoch)
    {
        const DateTime dt = fromEpochMs(-123456);
        EXPECT_EQ(dt.year, 1970);
        EXPECT_EQ(dt.month, 1);
        EXPECT_EQ(dt.day, 1);
    }

} // namespace
