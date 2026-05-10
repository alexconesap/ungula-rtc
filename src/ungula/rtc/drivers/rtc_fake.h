// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <stdint.h>

#include "ungula/rtc/i_rtc.h"

/// @brief Header-only test fake for `IRtc`.
///
/// Drop-in for any code that takes `IRtc*`. Holds an in-memory epoch and
/// validity flag, exposes call counters, and lets tests script every
/// operation. Lives under `drivers/` so every test can pick it up with
/// one include — same pattern as `EncoderFake` and `MultiplexerFake`.
///
/// Tests use this fake to detect interface drift: every pure-virtual on
/// `IRtc` must be implemented here, otherwise the file fails to compile
/// and the contract test fires immediately.

namespace ungula::rtc::drivers
{

    class RtcFake final : public IRtc {
    public:
        RtcFake(const char *name = "fake", ungula::hal::multiplexer::IMultiplexer *multiplexer = nullptr)
                : IRtc("FAKE", name, multiplexer)
        {
        }

        // ---- Driver contract ----

        bool begin(uint8_t multiplexerChannel = 0) override
        {
            ++beginCallCount_;
            multiplexerChannel_ = multiplexerChannel;
            isInitialized_ = true;
            if (!beginResult_) {
                setInitializationStatus(Error::BeginFailed);
            }
            return beginResult_;
        }

        bool isConnected() override
        {
            ++isConnectedCallCount_;
            return isConnectedResult_;
        }

        bool isTimeValid() override
        {
            ++isTimeValidCallCount_;
            if (!selectMultiplexerChannel()) {
                return false;
            }
            return timeValid_;
        }

        bool readEpochMs(epoch_ms_t &out) override
        {
            ++readEpochCallCount_;
            if (!selectMultiplexerChannel()) {
                return false;
            }
            if (!readResult_) {
                last_error_ = Error::I2CReadError;
                return false;
            }
            out = scriptedEpochMs_;
            clearLastError();
            return true;
        }

        bool writeEpochMs(epoch_ms_t epochMs) override
        {
            ++writeEpochCallCount_;
            if (!selectMultiplexerChannel()) {
                return false;
            }
            if (!writeResult_) {
                last_error_ = Error::I2CWriteError;
                return false;
            }
            scriptedEpochMs_ = epochMs;
            timeValid_ = true; // a successful write always validates
            clearLastError();
            return true;
        }

        // ---- Test knobs ----

        void setBeginResult(bool ok)
        {
            beginResult_ = ok;
        }
        void setIsConnected(bool ok)
        {
            isConnectedResult_ = ok;
        }
        void setTimeValid(bool valid)
        {
            timeValid_ = valid;
        }
        void setEpochMs(epoch_ms_t v)
        {
            scriptedEpochMs_ = v;
        }
        void setReadResult(bool ok)
        {
            readResult_ = ok;
        }
        void setWriteResult(bool ok)
        {
            writeResult_ = ok;
        }

        // ---- Inspectors ----

        epoch_ms_t scriptedEpochMs() const
        {
            return scriptedEpochMs_;
        }
        uint32_t beginCallCount() const
        {
            return beginCallCount_;
        }
        uint32_t isConnectedCallCount() const
        {
            return isConnectedCallCount_;
        }
        uint32_t isTimeValidCallCount() const
        {
            return isTimeValidCallCount_;
        }
        uint32_t readEpochCallCount() const
        {
            return readEpochCallCount_;
        }
        uint32_t writeEpochCallCount() const
        {
            return writeEpochCallCount_;
        }

    private:
        bool beginResult_ = true;
        bool isConnectedResult_ = true;
        bool readResult_ = true;
        bool writeResult_ = true;
        bool timeValid_ = false; // a fresh chip has never been set
        epoch_ms_t scriptedEpochMs_ = 0;

        uint32_t beginCallCount_ = 0;
        uint32_t isConnectedCallCount_ = 0;
        uint32_t isTimeValidCallCount_ = 0;
        uint32_t readEpochCallCount_ = 0;
        uint32_t writeEpochCallCount_ = 0;
    };

} // namespace ungula::rtc::drivers
