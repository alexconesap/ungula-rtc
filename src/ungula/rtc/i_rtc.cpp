// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include "i_rtc.h"

#include <stdarg.h>
#include <stdio.h>

#include <emblogx/logger.h>

namespace ungula::rtc
{

    namespace
    {
        constexpr size_t LOG_BODY_CAPACITY = 96;
        constexpr size_t LOG_PREFIX_CAPACITY = 64;
    } // namespace

    size_t IRtc::formatLogPrefix(char *buf, size_t bufSize) const
    {
        if (buf == nullptr || bufSize == 0) {
            return 0;
        }
        const int n = snprintf(buf, bufSize, "[%s %s @0x%02X]", model_ != nullptr ? model_ : "?",
                               name_ != nullptr ? name_ : "?", address_);
        return (n < 0) ? 0 : static_cast<size_t>(n);
    }

#define UNGULA_RTC_DEFINE_LOG_HELPER(NAME, EMIT) \
    void IRtc::NAME(const char *fmt, ...) const  \
    {                                            \
        if (!loggingEnabled_) {                  \
            return;                              \
        }                                        \
        char prefix[LOG_PREFIX_CAPACITY];        \
        formatLogPrefix(prefix, sizeof(prefix)); \
        char body[LOG_BODY_CAPACITY];            \
        va_list ap;                              \
        va_start(ap, fmt);                       \
        vsnprintf(body, sizeof(body), fmt, ap);  \
        va_end(ap);                              \
        EMIT(LOG_MODULE, "%s %s", prefix, body); \
    }

    UNGULA_RTC_DEFINE_LOG_HELPER(logInfof, log_info_m)
    UNGULA_RTC_DEFINE_LOG_HELPER(logWarnf, log_warn_m)
    UNGULA_RTC_DEFINE_LOG_HELPER(logErrorf, log_error_m)
    UNGULA_RTC_DEFINE_LOG_HELPER(logDebugf, log_debug_m)

#undef UNGULA_RTC_DEFINE_LOG_HELPER

    bool IRtc::selectMultiplexerChannel()
    {
        if (!isInitialized_) {
            setStatus(Error::NotInitialized);
            logErrorf("not initialised, call begin() first");
            return false;
        }
        if (multiplexer_ == nullptr) {
            clearLastError();
            return true;
        }
        if (!multiplexer_->selectChannel(multiplexerChannel_)) {
            setStatus(Error::MultiplexerError);
            return false;
        }
        clearLastError();
        return true;
    }

    const char *IRtc::getLastErrorAsStr() const
    {
        switch (last_error_) {
        case Error::None:
            return "No errors reported";
        case Error::NotInitialized:
            return "is not initialised. Call begin() first.";
        case Error::BeginFailed:
            return "failed during initialisation (begin).";
        case Error::NotConnected:
            return "not connected / not found.";
        case Error::MultiplexerError:
            return "multiplexer channel-select failed.";
        case Error::TimeNotValid:
            return "time is not valid (oscillator stopped or never set).";
        case Error::I2CReadError:
            return "I2C read error.";
        case Error::I2CWriteError:
            return "I2C write error.";
            // No default — let the compiler flag any new enum values
            // we forget to map here.
        }
        return "";
    }

} // namespace ungula::rtc
