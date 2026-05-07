// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once
#ifndef __cplusplus
#error UngulaRtc requires a C++ compiler
#endif

// Ungula RTC Library — battery-backed real-time-clock drivers.
//
// Depends on UngulaCore (ITimeProvider, epoch_ms_t, monotonic millis())
// and UngulaHal (I2cMaster, optional IMultiplexer). Including those
// umbrellas first ensures the Arduino CLI discovers their include paths
// before our headers reach the compiler.
#include <ungula/core.h>
#include <ungula/hal.h>

// Chip-neutral interface — implemented by every concrete driver.
#include "ungula/rtc/i_rtc.h"

// ITimeProvider adapter — install via ungula::core::time::setTimeProvider.
#include "ungula/rtc/rtc_time_provider.h"
