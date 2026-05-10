// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include <gtest/gtest.h>

#include <cstdint>

#include <ungula/rtc/detail/bcd.h>

namespace
{

    using ungula::rtc::detail::bcdToBin;
    using ungula::rtc::detail::binToBcd;

    TEST(Bcd, RoundTripsAcrossEveryValidValue)
    {
        // 0..99 covers every BCD-representable value the chips can store.
        // Each must survive a binToBcd → bcdToBin round trip without loss.
        for (uint8_t v = 0; v < 100; ++v) {
            EXPECT_EQ(bcdToBin(binToBcd(v)), v) << "value " << static_cast<int>(v);
        }
    }

    TEST(Bcd, EncodesKnownEdges)
    {
        EXPECT_EQ(binToBcd(0), 0x00);
        EXPECT_EQ(binToBcd(9), 0x09);
        EXPECT_EQ(binToBcd(10), 0x10); // tens digit kicks in
        EXPECT_EQ(binToBcd(59), 0x59);
        EXPECT_EQ(binToBcd(99), 0x99);
    }

    TEST(Bcd, DecodesKnownEdges)
    {
        EXPECT_EQ(bcdToBin(0x00), 0);
        EXPECT_EQ(bcdToBin(0x09), 9);
        EXPECT_EQ(bcdToBin(0x10), 10);
        EXPECT_EQ(bcdToBin(0x59), 59);
        EXPECT_EQ(bcdToBin(0x99), 99);
    }

    TEST(Bcd, OutOfRangeBinReturnsZero)
    {
        // The chips can't produce > 99; refuse to encode rather than
        // round-trip silently to a bogus value.
        EXPECT_EQ(binToBcd(100), 0);
        EXPECT_EQ(binToBcd(255), 0);
    }

    // bcdToBin(0xFF) etc. is "garbage in, garbage out" by design — the
    // chip reads use mask bits to strip reserved bits before calling the
    // codec, so every byte passed in is in range. No test for invalid
    // BCD input.

} // namespace
