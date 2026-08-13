# UngulaRtc (`lib_rtc`)

LLM-oriented public-API reference for the RTC library. For the
human-facing overview see [`README.md`](README.md). For project-wide
rules see `code/CLAUDE.md`.

The library provides a chip-neutral interface (`ungula::rtc::IRtc`) and
concrete drivers (`Ds3231`, `Ds1307`) plus an `RtcTimeProvider` adapter
that plugs into `ungula::core::time::setTimeProvider(...)` so wall-clock
calls (`now`, `nowLocal`, `formatLocal`, ...) flow through the chip
automatically. The multiplexer is optional — every driver works with
`multiplexer == nullptr` (direct-connect) or with an `IMultiplexer*`.

---

## LLM quick map

- **Primary include**: `#include <ungula/rtc.h>`.
- **Arduino discovery include**: `#include <ungula_rtc.h>` (forwarder only; host code should keep using the real header).
- **Namespace root**: `ungula::rtc`.
- **Own source minimum**: `C++17`.
- **Effective minimum for consumers**: `C++17`.
- **Dependency impact**: Declared internal dependencies `UngulaCore` and `UngulaHal` are `C++17`.
- **Supported architectures**: `esp32,esp32-s3`.
- **Read order for coding agents**: `Usage` (working patterns) -> `API` (symbols/signatures) -> `Lifecycle`/`Error handling`/`Threading` notes in this file.

### Use-case index

- [Use case: DS3231 plugged into the time API](#use-case-ds3231-plugged-into-the-time-api)
- [Use case: detect "battery dead, time lost" on boot](#use-case-detect-battery-dead-time-lost-on-boot)
- [Use case: DS1307 with API-identical code](#use-case-ds1307-with-api-identical-code)
- [Use case: chip behind a multiplexer](#use-case-chip-behind-a-multiplexer)
- [Use case: per-instance debugging](#use-case-per-instance-debugging)

### LLM rules

- Use only symbols and include paths documented in this file; do not infer extra public API from implementation files.
- Prefer the use-case patterns here over ad-hoc rewrites; keep dependency wiring and lifecycle order identical unless the task explicitly changes API design.
- Treat headers under `detail/`, `platform/`, and `platforms/` as internal unless this document calls them out as public.
- If required behavior is missing from the documented API, report the gap explicitly instead of inventing new public symbols.


## Usage

### Use case: DS3231 plugged into the time API

```cpp
#include <ungula/hal/i2c/i2c_master.h>
#include <ungula/rtc/drivers/ds3231.h>
#include <ungula/rtc/rtc_time_provider.h>
#include <ungula/core/time/time.h>

namespace tc = ungula::core::time;

ungula::hal::i2c::I2cMaster bus(0);
ungula::rtc::drivers::Ds3231 chip(bus);
ungula::rtc::RtcTimeProvider clock(chip);

void setup() {
    bus.begin(21, 22, 400000);
    chip.begin();
    tc::setTimeProvider(&clock);
    tc::setTimezone(tc::tz::Timezone::CET);
}
```

When to use this: any host that needs wall-clock time across reboots or
without WiFi.

### Use case: detect "battery dead, time lost" on boot

```cpp
if (!chip.isTimeValid()) {
    // Host decides: NTP, setup screen, build-time default.
    chip.writeEpochMs(getInitialTimeFromSomewhere());
    clock.invalidateCache();
}
```

When to use this: every boot — the validity flag is the chip's signal
that the oscillator stopped.

### Use case: DS1307 with API-identical code

```cpp
ungula::rtc::drivers::Ds1307 chip(bus);   // same calls as Ds3231
chip.begin();
chip.writeEpochMs(epoch);
```

When to use this: cheaper boards. Same API, lower precision.

### Use case: chip behind a multiplexer

```cpp
ungula::hal::multiplexer::drivers::MultiplexerTCA9548 mux70(0x70, bus);
ungula::rtc::drivers::Ds3231 chip(bus, &mux70);
mux70.begin();
chip.begin(/*channel=*/3);
```

When to use this: shared I2C bus where the RTC's address (0x68) clashes
with another device.

### Use case: per-instance debugging

```cpp
chip.enableLogging();   // EmblogX module = "rtc"
```

When to use this: one chip is misbehaving and you want its lines without
flooding the log with the rest.

---

## Public types

| Type | Header | Purpose |
| ---- | ------ | ------- |
| `ungula::rtc::IRtc` | `ungula/rtc/i_rtc.h` | Chip-neutral interface, multiplexer optional |
| `ungula::rtc::Status` (enum) | same | `Ok`, `InitializationError`, `Error` |
| `ungula::rtc::Error` (enum) | same | `None`, `NotInitialized`, `BeginFailed`, `NotConnected`, `MultiplexerError`, `TimeNotValid`, `I2CReadError`, `I2CWriteError`. `TimeNotValid` and `NotConnected` are declared but no driver sets them today — do not branch on them |
| `ungula::rtc::epoch_ms_t` | same | Alias of `ungula::core::time::epoch_ms_t` (signed int64) |
| `ungula::rtc::RtcTimeProvider` | `ungula/rtc/rtc_time_provider.h` | `ITimeProvider` adapter with cache TTL |
| `ungula::rtc::drivers::Ds3231` | `ungula/rtc/drivers/ds3231.h` | DS3231 TCXO RTC |
| `ungula::rtc::drivers::Ds1307` | `ungula/rtc/drivers/ds1307.h` | DS1307 RTC (CH-bit validity) |
| `ungula::rtc::drivers::RtcFake` | `ungula/rtc/drivers/rtc_fake.h` | Header-only test fake |
| `ungula::rtc::detail::DateTime` | `ungula/rtc/detail/datetime_codec.h` | Y/M/D h:m:s tuple, internal use |

---

## `ungula::rtc::IRtc`

### Construction

```cpp
IRtc(const char* model, const char* name, IMultiplexer* multiplexer);
```

- All three pointers are borrowed.
- `multiplexer == nullptr` is a first-class direct-connect deployment.

### Identity

- `getName()`, `getModel()` — borrowed pointers passed at construction.
- `getAddress()` — set to chip default during `begin()`.
- `hasMultiplexer()` — `true` iff `multiplexer != nullptr`.

### Lifecycle

- **`virtual bool begin(uint8_t multiplexerChannel = 0) = 0`**
  Drivers must:
  1. Set `multiplexerChannel_`, `address_`.
  2. Set `isInitialized_ = true` (even on failure, so subsequent calls report the actual error rather than `NotInitialized`).
  3. `selectMultiplexerChannel()` — works in both deployments.
  4. `isConnected()` to probe the chip — return `BeginFailed` on miss.

### Time

- **`bool isConnected()`** — bus probe (zero-length write).
- **`bool isTimeValid()`** — true iff the chip's "I lost my time" flag is clear (DS3231 OSF, DS1307 CH). It returns `false` for two different reasons — flag set, or the register read failed — so check `getLastError()` when you need to tell them apart (`I2CReadError` on a failed read; unchanged when the flag itself is set).
- **`bool readEpochMs(epoch_ms_t& out)`** — UTC epoch ms; `false` on multiplexer error, I2C read error, or before `begin()`, and `out` is left untouched in those cases. **It returns `true` with `out == 0` when the registers decode to an impossible date** (month 0, day 40, ...), which is exactly what a never-set chip contains. Pair it with `isTimeValid()`, or treat `out == 0` as "no time" — `0` is also a legal 1970-01-01.
- **`bool writeEpochMs(epoch_ms_t value)`** — write the wall clock and clear the validity flag. Rejects anything outside 2000..2099 (the chips store a two-digit year) and reports it as `Error::I2CWriteError`, even though nothing was put on the wire.

### Status

- `getLastError()`, `getLastErrorAsStr()`, `clearLastError()`,
  `setStatus(Error)`, `setInitializationStatus(Error)`.

### Logging (off by default)

- `enableLogging()`, `disableLogging()`, `isLoggingEnabled()`.
  EmblogX module tag `"rtc"`. Per-instance.

### Protected helpers

- **`selectMultiplexerChannel()`** — drivers call this before any I2C transaction. No-op when no multiplexer; otherwise calls `mux->selectChannel(channel)`. Sets `Error::MultiplexerError` on failure, `Error::NotInitialized` if `begin()` was never called.
- **`logInfof / logWarnf / logErrorf / logDebugf`** — printf-style helpers that prepend the per-instance prefix automatically.
- **`virtual size_t formatLogPrefix(char*, size_t)`** — overrideable prefix builder. Default: `[<model> <name> @0x<addr>]`.

---

## `ungula::rtc::RtcTimeProvider`

```cpp
explicit RtcTimeProvider(IRtc& chip);
```

Implements `ungula::core::time::ITimeProvider`. Install via `setTimeProvider(&provider)`.

### Cache

- **`int64_t nowMs() const override`** — cache-anchored: one chip read per TTL window, served via `cached_epoch_ms + (millis() - anchor_tick)` between reads. Returns `0` while the cache is invalid — always gate on `isValid()` (`ungula::core::time::now()` already does).
- While the chip is unreachable or reports invalid time the cache never becomes valid, so **every** `nowMs()` / `isValid()` call retries the chip regardless of the TTL. Keep that in mind before calling `now()` in a tight loop on a board whose RTC may be missing.
- **`bool isValid() const override`** — false until the first successful chip read; false after a chip-side `isTimeValid() == false`; false after I2C read failure. When false, `ungula::core::time::now()` falls back to local `millis()` (same contract as `NtpTimeProvider`).
- **`setRefreshIntervalMs(duration_ms_t)`** — TTL in ms. Default 1000. `0` disables caching.
- **`refreshIntervalMs()` const** — current TTL.
- **`invalidateCache()`** — force the next `nowMs()`/`isValid()` to re-read the chip. Use after `chip.writeEpochMs(...)` to avoid serving the stale cached value for up to TTL ms.

---

## `ungula::rtc::drivers::Ds3231`

```cpp
Ds3231(I2cMaster& bus, IMultiplexer* multiplexer = nullptr,
       const char* name = "main");
```

- Fixed I2C address `0x68` (`DS3231_DEFAULT_ADDRESS`).
- TCXO-compensated, ±2 ppm typical.
- `isTimeValid()` reads status register `0x0F` bit 7 (OSF). The bit is set on every fresh power-up before the battery has run.
- `writeEpochMs()` writes the seven time registers and clears OSF in a separate read-modify-write cycle. If the time write lands but the OSF clear fails, the call returns `false` — the clock is set but still reports invalid.
- The read path assumes the chip is in **24-hour mode** and masks the hour register with `0x3F`. It does not inspect the 12/24 bit, so a chip left in 12-hour mode by other firmware decodes the hour wrong until the first `writeEpochMs()` (which always writes 24-hour mode).
- The century bit (month register bit 7) is ignored, and the year is decoded as `2000 + YY`. Range is 2000..2099.

---

## `ungula::rtc::drivers::Ds1307`

```cpp
Ds1307(I2cMaster& bus, IMultiplexer* multiplexer = nullptr,
       const char* name = "main");
```

- Fixed I2C address `0x68` (`DS1307_DEFAULT_ADDRESS`). Same address as DS3231 — only one of these on a given bus.
- No temperature compensation; expect seconds-per-day drift.
- `isTimeValid()` reads register `0x00` bit 7 (CH). The chip ships with CH set; the oscillator only starts running after a successful `writeEpochMs()`.
- `writeEpochMs()` writes seven registers in a single block; bit 7 of seconds is naturally written as 0, which both stores the seconds value and clears CH.
- Same 24-hour-mode assumption and same 2000..2099 range as the DS3231.
- A never-set DS1307 reads back all zeros, which decodes to month 0 / day 0 — an impossible date. `readEpochMs()` reports `true` with `out == 0` in that case, so `isTimeValid()` is the check that matters on boot.

---

## `ungula::rtc::drivers::RtcFake`

Header-only test fake.

### Test knobs

- `setBeginResult(bool)`, `setIsConnected(bool)`, `setReadResult(bool)`, `setWriteResult(bool)`.
- `setTimeValid(bool)` — drives `isTimeValid()`.
- `setEpochMs(epoch_ms_t)` — value returned by `readEpochMs`.

### Inspectors

- `scriptedEpochMs()`, `beginCallCount()`, `isConnectedCallCount()`,
  `isTimeValidCallCount()`, `readEpochCallCount()`, `writeEpochCallCount()`.

### Drift-detection role

`test_rtc.cpp::IRtcContract.FakeImplementsEveryPureVirtual` calls every pure virtual through an `IRtc*`. Renaming or re-typing any method without updating the fake breaks the build.

---

## Lifecycle and error handling

- **Initialisation order**: `bus.begin()` → `multiplexer.begin()` (if any) → `chip.begin()` → `setTimeProvider(&provider)`.
- **No exceptions**. Failures surface as `false` returns, `0` outputs, or non-`Ok` enum values.
- **`readEpochMs()` returns `false`** and leaves `out` untouched on multiplexer error, I2C read error, or before `begin()`. Caller should check `getLastError()` for the reason. A garbage-but-readable register set is *not* an error path: see the `IRtc` section.
- **Validity flag is the recovery signal**: a host that wants self-healing time should always check `isTimeValid()` on boot and re-write from a fresh source when false.
- **Year range is 2000..2099** on both chips. `writeEpochMs()` refuses anything else instead of truncating.

## Threading / hardware

- Drivers are **not thread-safe** — use one instance per task or guard with a mutex. The shared `I2cMaster` is also single-task by design.
- The library issues no delays of its own; every transaction is a single `I2cMaster` call. Time comes from `ungula::core::time::millis()` in the provider only. No Arduino, no `vTaskDelay`.
- `RtcTimeProvider::nowMs()` does at most one chip read per TTL window **while the cache is valid**. An unreachable or unset chip re-reads on every call.
- The cache is plain mutable member state on the provider instance — no globals, no locks. One provider per chip.
- `i_rtc.cpp` links against **EmblogX** for the log helpers even when logging is off. It is not listed in `library.properties` `depends=`, so the host project must provide it.

## LLM usage rules

- Use `IRtc*` in code that consumes RTCs. Do not depend on the concrete driver type.
- Pass `nullptr` for the multiplexer when the chip is direct-connect. Do not invent a "null multiplexer" wrapper.
- Always check `chip.isTimeValid()` on boot before trusting the time. Re-write from NTP / user / default when false. This is the check that catches a never-set chip — `readEpochMs()` alone will happily report success with epoch 0.
- Do not branch on `Error::TimeNotValid`; no driver produces it.
- Use `ungula::core::time::*` for any pacing — never `delay()` / `delayMicroseconds()`.
- Do not move time abstractions out of UngulaCore into this library — `ITimeProvider` and `epoch_ms_t` belong to core; this library is one of several sources for it.
- Logging is per-instance (`chip.enableLogging()`), module tag fixed (`rtc`).
