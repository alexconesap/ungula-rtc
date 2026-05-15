// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ungula/core/time/time.h"
#include "ungula/hal/multiplexer/i_multiplexer.h"

/// @brief Chip-neutral interface for battery-backed real-time clocks.
///
/// Concrete drivers (DS3231, DS1307, ...) inherit `IRtc` and implement
/// the chip-specific bits. Code that consumes RTCs depends only on this
/// interface. To plug an RTC into the rest of the codebase, wrap it in
/// `RtcTimeProvider` and call `ungula::core::time::setTimeProvider(...)`.
///
/// ## Multiplexer is optional
///
/// Same pattern as `IEncoder`: pass an `IMultiplexer*` and a channel to
/// `begin(channel)`, or pass `nullptr` for direct-connect deployments.
/// Both work through the same code path.
///
/// ## Time validity
///
/// Every chip exposes some form of "have I ever been set / have I lost
/// my battery?" signal:
///   - DS3231: oscillator-stop flag (OSF) in status register 0x0F bit 7.
///   - DS1307: clock-halt (CH) bit in seconds register 0x00 bit 7.
///
/// `isTimeValid()` collapses both into a single boolean. The host calls
/// it on boot; if false, it should pull the time from somewhere (NTP,
/// user setup, build-time default) and call `writeEpochMs()`.

namespace ungula::rtc
{

/// Same alias as `ungula::core::time::epoch_ms_t` — kept here to
/// keep the interface header self-sufficient.
using epoch_ms_t = ungula::core::time::epoch_ms_t;

enum class Status : uint8_t {
        Ok = 0,
        InitializationError,
        Error,
};

enum class Error : uint8_t {
        None = 0,
        NotInitialized,
        BeginFailed,
        NotConnected,
        MultiplexerError,
        TimeNotValid, // OSF / CH bit set; read may be stale
        I2CReadError,
        I2CWriteError,
};

/// @brief Abstract base for all RTC chips.
class IRtc {
    public:
        /// @param model       Short label, e.g. "DS3231". Borrowed.
        /// @param name        Caller-chosen tag, e.g. "main". Borrowed.
        /// @param multiplexer Optional. `nullptr` means "wired direct".
        IRtc(const char *model, const char *name,
             ungula::hal::multiplexer::IMultiplexer *multiplexer)
                : model_(model)
                , name_(name)
                , multiplexer_(multiplexer)
        {
        }

        virtual ~IRtc() = default;

        IRtc(const IRtc &) = delete;
        IRtc &operator=(const IRtc &) = delete;

        // ---- Identity ----

        const char *getName() const
        {
                return name_;
        }
        const char *getModel() const
        {
                return model_;
        }

        uint8_t getAddress() const
        {
                return address_;
        }
        bool hasMultiplexer() const
        {
                return multiplexer_ != nullptr;
        }

        // ---- Status helpers (concrete on the base) ----

        Error getLastError() const
        {
                return last_error_;
        }
        const char *getLastErrorAsStr() const;
        void clearLastError()
        {
                setStatus(Error::None);
        }
        void setStatus(Error error)
        {
                last_error_ = error;
                status_ = (error == Error::None) ? Status::Ok : Status::Error;
        }
        void setInitializationStatus(Error error)
        {
                last_error_ = error;
                status_ = (error == Error::None) ? Status::Ok : Status::InitializationError;
        }

        // ---- Driver contract ----

        /// @brief Initialise the chip. Drivers must:
        ///   1. Capture the multiplexer channel.
        ///   2. Set `isInitialized_ = true` (even on failure).
        ///   3. Probe the chip — return false on `BeginFailed`.
        virtual bool begin(uint8_t multiplexerChannel = 0) = 0;

        /// @brief Probe the chip. Zero-length write to the I2C address.
        virtual bool isConnected() = 0;

        /// @brief True iff the chip has been set since power-on AND
        /// the oscillator hasn't stopped. False after a battery loss
        /// (DS3231 OSF, DS1307 CH).
        virtual bool isTimeValid() = 0;

        /// @brief Read the chip's wall clock as Unix epoch ms (UTC).
        /// On failure returns `false` and leaves `out` untouched —
        /// caller should check `getLastError()`.
        virtual bool readEpochMs(epoch_ms_t &out) = 0;

        /// @brief Write the wall clock and clear the "invalid" flag
        /// (OSF / CH). Use this after fetching time from NTP, a user
        /// setup screen, or any other source.
        virtual bool writeEpochMs(epoch_ms_t epochMs) = 0;

        // ---- Optional logging (off by default) ----

        void enableLogging()
        {
                loggingEnabled_ = true;
        }
        void disableLogging()
        {
                loggingEnabled_ = false;
        }
        bool isLoggingEnabled() const
        {
                return loggingEnabled_;
        }

    protected:
        /// @brief Drivers call this before any I2C transaction.
        /// Returns true when the bus is ready (multiplexer channel
        /// selected, or no multiplexer is in use). Sets
        /// `Error::MultiplexerError` on failure, `Error::NotInitialized`
        /// if `begin()` was never called.
        bool selectMultiplexerChannel();

        // EmblogX module tag for every line emitted by the hierarchy.
        static constexpr const char *LOG_MODULE = "rtc";

        bool shouldLog() const
        {
                return loggingEnabled_;
        }

        /// @brief Per-instance log helpers. Each prepends the prefix
        /// produced by `formatLogPrefix()` so drivers never repeat
        /// the `[<model> <name> @0x<addr>]` boilerplate. No-op when
        /// logging is disabled. Format-checked.
        void logInfof(const char *fmt, ...) const __attribute__((format(printf, 2, 3)));
        void logWarnf(const char *fmt, ...) const __attribute__((format(printf, 2, 3)));
        void logErrorf(const char *fmt, ...) const __attribute__((format(printf, 2, 3)));
        void logDebugf(const char *fmt, ...) const __attribute__((format(printf, 2, 3)));

        /// @brief Build the per-instance log prefix into `buf`.
        virtual size_t formatLogPrefix(char *buf, size_t bufSize) const;

        // ---- Construction parameters ----
        const char *model_;
        const char *name_;
        ungula::hal::multiplexer::IMultiplexer *multiplexer_; // nullable

        // ---- Run-time state ----
        bool isInitialized_ = false;
        uint8_t address_ = 0x00;
        uint8_t multiplexerChannel_ = 0;

        Status status_ = Status::Ok;
        Error last_error_ = Error::None;

    private:
        bool loggingEnabled_ = false;
};

} // namespace ungula::rtc
