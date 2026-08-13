# UngulaRtc

> **Battery-backed real-time clocks for embedded C++** — DS3231 and DS1307 today, more chips on the same `IRtc` interface tomorrow.

> **LLM usage note:** if this library is consumed from a coding AI workflow, explicitly point the agent to `API.md` first. `API.md` is the LLM-facing contract (public API + examples + constraints) and avoids wasting time/tokens scanning source files and this human-oriented README.

> **Warning - Active Development:** This library is under active architecture work to support multiple projects in parallel. Its structure is not finalized yet and may change without notice while this work is in progress. Updates are currently frequent (often daily). Target for structural freeze and stable `v1.0.0`: **June 2026**.

The host project sets the time once (from NTP, a setup screen, anything),
and the RTC keeps wall-clock time across reboots, brown-outs and battery
swaps. Plug the chip into the rest of the codebase via the existing
`ungula::core::time::setTimeProvider(...)` hook and `now()` / `nowLocal()` /
`formatLocal()` start returning real wall-clock time.

## Table of Contents

- [C++ Compatibility](#c-compatibility)
- [Features](#features)
- [Supported chips](#supported-chips)
- [Dependencies](#dependencies)
- [Architecture](#architecture)
- [Quick start — DS3231 plugged into the time API](#quick-start--ds3231-plugged-into-the-time-api)
- [Setting the initial time](#setting-the-initial-time)
  - [From NTP, after WiFi comes up](#from-ntp-after-wifi-comes-up)
  - [From a setup screen / build-time default](#from-a-setup-screen--build-time-default)
- [Multiplexer is optional](#multiplexer-is-optional)
- [Known limits](#known-limits)
- [Logging](#logging)
- [Testing](#testing)
- [Acknowledgements](#acknowledgements)
- [License](#license)
- [Arduino CLI symlink note (rarely relevant)](#arduino-cli-symlink-note-rarely-relevant)

## C++ Compatibility

- **Own source minimum**: `C++17`.
- **Effective minimum for consumers**: `C++17`.
- **Dependency impact**: Declared internal dependencies `UngulaCore` and `UngulaHal` are `C++17`.

## Features

- Chip-neutral `IRtc` interface; same code path with or without a multiplexer.
- `RtcTimeProvider` adapter into `ungula::core::time::ITimeProvider` — no host changes beyond `setTimeProvider(&clock)`.
- Power-loss detection (DS3231 OSF, DS1307 CH) surfaced as a single `isTimeValid()` boolean. Hosts decide whether to re-prompt the user, fetch NTP, etc.
- Cached `now()`: one chip read per second by default, configurable via `setRefreshIntervalMs()`. Disable for always-fresh reads.
- Per-instance EmblogX logging toggle, off by default. Module tag `rtc`.
- Header-only `RtcFake` for host tests; locks the interface so a future signature change cannot silently break drivers.

## Supported chips

| Chip   | Bus | Validity flag | Driver |
| ------ | --- | ------------- | ------ |
| DS3231 | I2C @ 0x68 | OSF (status reg 0x0F bit 7) | `ungula::rtc::drivers::Ds3231` |
| DS1307 | I2C @ 0x68 | CH  (seconds reg 0x00 bit 7) | `ungula::rtc::drivers::Ds1307` |

Both chips share register layout `0x00..0x06` (BCD seconds, minutes, hours, day-of-week, date, month, year) so the BCD codec is shared. Only the validity-flag check differs — hidden behind `isTimeValid()`.

## Dependencies

- **UngulaCore** — `ITimeProvider`, `epoch_ms_t`, monotonic `millis()` for cache anchoring, the time API itself.
- **UngulaHal** — `i2c::I2cMaster` for the wire, optional `multiplexer::IMultiplexer` for shared buses.
- **EmblogX** — runtime diagnostics (logging is off by default).

`ungula::core::time` lives in **UngulaCore** and is shared across every library — `lib_rtc` is one of several pluggable sources for it (NTP from `lib_net` is another). Nothing was moved into `lib_rtc`.

## Architecture

```
ungula::rtc
├── IRtc                          ← chip-neutral interface
│    ├── isTimeValid()            ← OSF / CH detection
│    ├── readEpochMs(out)         ← UTC epoch ms
│    ├── writeEpochMs(value)      ← also clears OSF / CH
│    ├── enableLogging() / disableLogging()
│    └── selectMultiplexerChannel() — no-op when no mux
├── RtcTimeProvider               ← adapts IRtc → ITimeProvider, with cache TTL
├── detail/
│    ├── bcd.h                    ← BCD ↔ binary helpers
│    └── datetime_codec.h         ← epoch_ms_t ↔ Y/M/D h:m:s
└── drivers/
     ├── Ds3231                   ← OSF-based isTimeValid
     ├── Ds1307                   ← CH-based isTimeValid
     └── RtcFake                  ← header-only test fake
```

Drivers do not include `<Wire.h>` or any Arduino API. Time and pacing go through `ungula::core::time`.

## Quick start — DS3231 plugged into the time API

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
    bus.begin(/*sda=*/21, /*scl=*/22, /*hz=*/400000);
    chip.begin();
    tc::setTimeProvider(&clock);             // wall-clock now flows through the chip
    tc::setTimezone(tc::tz::Timezone::CET);  // device deployed in Barcelona
}

void loop() {
    char ts[20];
    tc::formatLocal(ts, sizeof(ts));   // "2026-04-23 15:32:11"
    // ...
}
```

That's the whole integration. `tc::now()`, `tc::nowLocal()`,
`tc::nowInTz(...)`, `tc::formatUtc()`, `tc::formatLocal()` —
everything that already works against an `ITimeProvider` keeps working.

## Setting the initial time

A fresh chip reports `isTimeValid() == false`. The host decides where the time comes from. Two common patterns:

### From NTP, after WiFi comes up

```cpp
#include <ungula/rtc.h>

if (!chip.isTimeValid()) {
    if (auto epochMs = ntpFetchOnce()) {
        chip.writeEpochMs(*epochMs);   // also clears OSF / CH
        clock.invalidateCache();        // drop cached "invalid"
    }
}
```

### From a setup screen / build-time default

```cpp
#include <ungula/rtc.h>

if (!chip.isTimeValid()) {
    showSetupScreen();          // host code, asks the user
    chip.writeEpochMs(userEpochMs);
    clock.invalidateCache();
}
```

After the write, every subsequent `tc::now()` flows through the chip until power loss. After a battery-replacement boot, `isTimeValid()` returns false again — the same recovery path applies.

## Multiplexer is optional

Same shape as encoders. If the chip lives behind a TCA9548:

```cpp
#include <ungula/rtc.h>

ungula::hal::multiplexer::drivers::MultiplexerTCA9548 mux70(0x70, bus);
ungula::rtc::drivers::Ds3231 chip(bus, &mux70);
mux70.begin();
chip.begin(/*channel=*/3);
```

Direct-connect (default):

```cpp
#include <ungula/rtc.h>

ungula::rtc::drivers::Ds3231 chip(bus);
chip.begin();   // multiplexer channel ignored
```

The driver handles `selectMultiplexerChannel()` internally before every transaction.

## Known limits

Worth knowing before you wire one of these in:

- **Year range 2000..2099.** Both chips store a two-digit year. The
  DS3231 century bit is ignored on read, and `writeEpochMs()` refuses
  anything outside the range instead of silently truncating.
- **24-hour mode is assumed.** The hour register is masked with `0x3F`
  and the 12/24 bit is not inspected. A chip left in 12-hour mode by
  other firmware decodes the hour wrong until the first
  `writeEpochMs()`, which always writes 24-hour mode.
- **`readEpochMs()` can succeed with epoch 0.** A never-set chip holds
  zeros, which decode to month 0 / day 0. The codec rejects the date and
  yields `0`, but the call still returns `true`. `isTimeValid()` is the
  boot check that catches it — do not skip it.
- **No timezone or DST handling here.** The chips hold UTC by
  convention; local time is `ungula::core::time`'s job
  (`setTimezone()`, `nowLocal()`, `formatLocal()`).
- **EmblogX is a link-time dependency** of `i_rtc.cpp` even with logging
  off, and it is not listed in `library.properties` `depends=`.

## Logging

Off by default. `enableLogging()` routes diagnostics through EmblogX with the module tag `rtc`. Per-instance — debugging one RTC doesn't pollute logs from other devices.

```cpp
#include <ungula/rtc.h>

chip.enableLogging();
```

Each line carries the prefix `[<model> <name> @0x<addr>]` automatically; drivers don't repeat it. Helpers used internally: `logInfof`, `logWarnf`, `logErrorf`, `logDebugf`.

## Testing

Host-side tests live in `tests/`:

- `test_bcd.cpp` — BCD ↔ binary round-trip + edges.
- `test_datetime_codec.cpp` — epoch ↔ Y/M/D h:m:s round-trip, leap-year rules, out-of-range clamping. Anchored on `2023-11-14 22:13:20 UTC` so it cross-checks against the rest of the time stack.
- `test_rtc.cpp` — `IRtc` contract through `RtcFake`. Catches signature drift: every pure virtual is touched through an `IRtc*`. Both deployment modes (with/without multiplexer) covered.
- `test_rtc_time_provider.cpp` — cache TTL, `invalidateCache()`, fall-through to local `millis()` when the chip is invalid, end-to-end install via `tc::setTimeProvider`.
- `test_ds3231.cpp` / `test_ds1307.cpp` — driver smoke tests against the desktop I2C stub. Verify the drivers compile, link, behave as `IRtc`, and report the right errors when the bus is unreachable.

```bash
cd tests
cmake -S . -B build
cmake --build build
./build/test_ungula_rtc_codec
./build/test_ungula_rtc
./build/test_ungula_rtc_drivers
```

or just:

```bash
cd tests
chmod +x *sh
./1_build.sh
./2_run.sh
```

## Acknowledgements

Thanks to Claude and ChatGPT for helping on generating this documentation.

## License

MIT License — see [LICENSE](LICENSE) file.

---

## Arduino CLI symlink note (rarely relevant)

This library ships a flat forwarder header at `src/ungula_rtc.h` that just `#include`s `ungula/rtc.h`. `library.properties` `includes=` points at the forwarder. It only exists to work around an Arduino CLI quirk when the library is consumed through a symlink. Host code keeps including the real header (`#include <ungula/rtc.h>`).
