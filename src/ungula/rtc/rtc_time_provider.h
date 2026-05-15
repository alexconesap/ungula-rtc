// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <stdint.h>

#include "ungula/core/time/i_time_provider.h"
#include "ungula/core/time/time.h"
#include "ungula/rtc/i_rtc.h"

/// @brief `ITimeProvider` adapter that routes wall-clock reads through
/// an `IRtc`. Mirrors `lib_net::ntp::NtpTimeProvider` in shape so the
/// host project sees a uniform plug-in pattern across time sources.
///
/// Reading the chip costs a full I2C round-trip (~600 µs on DS3231 at
/// 400 kHz). To keep `now()` cheap on hot paths, the provider anchors on
/// one chip read and serves subsequent calls via:
///
///   cached_epoch_ms + (millis() - anchor_tick)
///
/// until the cache TTL expires. Default TTL is 1000 ms (RTC has 1-second
/// resolution; finer refresh would be wasted I2C traffic). Tune via
/// `setRefreshIntervalMs()`. `0` disables caching.
///
/// Usage:
///
/// ```cpp
///   namespace tc = ungula::core::time;
///   ungula::hal::i2c::I2cMaster bus(0);
///   ungula::rtc::drivers::Ds3231 chip(bus);
///   ungula::rtc::RtcTimeProvider clock(chip);
///   bus.begin(21, 22, 400000);
///   chip.begin();
///   tc::setTimeProvider(&clock);     // tc::now() now flows through the RTC
///   tc::setTimezone(tc::tz::Timezone::CET);
///   char ts[20];
///   tc::formatLocal(ts, sizeof(ts)); // "2026-04-23 15:32:11"
/// ```

namespace ungula::rtc
{

class RtcTimeProvider final : public ungula::core::time::ITimeProvider {
    public:
        using duration_ms_t = ungula::core::time::duration_ms_t;
        using tick_ms_t = ungula::core::time::tick_ms_t;

        /// @param chip  Borrowed reference. Caller owns it and must
        ///              keep it alive for the provider's lifetime.
        explicit RtcTimeProvider(IRtc &chip)
                : chip_(chip)
        {
        }

        // ---- ITimeProvider ----

        int64_t nowMs() const override;
        bool isValid() const override;

        // ---- Cache control ----

        /// @brief Override the cache TTL. `0` disables caching (every
        /// `now()` re-reads the chip).
        void setRefreshIntervalMs(duration_ms_t intervalMs)
        {
                refreshIntervalMs_ = intervalMs;
        }
        duration_ms_t refreshIntervalMs() const
        {
                return refreshIntervalMs_;
        }

        /// @brief Force the next `now()` to re-read the chip.
        /// Useful after a `chip.writeEpochMs(...)` to avoid serving
        /// stale cached values for up to TTL ms.
        void invalidateCache()
        {
                cachedValid_ = false;
        }

    private:
        void ensureCacheFresh() const;

        IRtc &chip_;
        duration_ms_t refreshIntervalMs_ = 1000;

        // Mutable because `nowMs()` / `isValid()` are logically const
        // but the cache updates on those calls.
        mutable epoch_ms_t cachedEpochMs_ = 0;
        mutable tick_ms_t cachedAnchorTick_ = 0;
        mutable bool cachedValid_ = false;
};

} // namespace ungula::rtc
