// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include "rtc_time_provider.h"

namespace ungula::rtc
{

    int64_t RtcTimeProvider::nowMs() const
    {
        ensureCacheFresh();
        if (!cachedValid_) {
            return 0;
        }
        const tick_ms_t nowTick = ungula::core::time::millis();
        const tick_ms_t elapsed = nowTick - cachedAnchorTick_;
        // Both anchors and ticks are signed 64-bit — diff is exact even
        // across long uptimes. See time_control.h for the rationale.
        return cachedEpochMs_ + elapsed;
    }

    bool RtcTimeProvider::isValid() const
    {
        ensureCacheFresh();
        return cachedValid_;
    }

    void RtcTimeProvider::ensureCacheFresh() const
    {
        const tick_ms_t now = ungula::core::time::millis();
        if (cachedValid_ && refreshIntervalMs_ > 0 && (now - cachedAnchorTick_) < refreshIntervalMs_) {
            return; // cache hit
        }

        // Re-read the chip. If the chip says "time not valid" or the
        // I2C read fails, drop validity so callers fall back to local
        // millis() — same contract as NtpTimeProvider.
        if (!chip_.isTimeValid()) {
            cachedValid_ = false;
            return;
        }
        epoch_ms_t fresh = 0;
        if (!chip_.readEpochMs(fresh)) {
            cachedValid_ = false;
            return;
        }
        cachedEpochMs_ = fresh;
        cachedAnchorTick_ = now;
        cachedValid_ = true;
    }

} // namespace ungula::rtc
