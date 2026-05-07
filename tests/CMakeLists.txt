cmake_minimum_required(VERSION 3.16)
project(ungula_rtc_lib_tests)

set(CMAKE_CXX_STANDARD 17)

set(FETCHCONTENT_BASE_DIR ${CMAKE_CURRENT_LIST_DIR}/vendor)

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

# ---------------------------------------------------------------------------
# Dependencies. UngulaCore (ITimeProvider, time API), UngulaHal (I2cMaster,
# multiplexer interface) and EmblogX (logging from inside i_rtc.cpp) — all
# sibling libraries during local development.
# ---------------------------------------------------------------------------
option(USE_LOCAL_DEPS "Use local sibling lib*/ directories" ON)

if(USE_LOCAL_DEPS)
    set(LIB_CORE_SRC    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src)
    set(LIB_HAL_SRC     ${CMAKE_CURRENT_LIST_DIR}/../../lib_hal/src)
    set(LIB_EMBLOGX_SRC ${CMAKE_CURRENT_LIST_DIR}/../../lib_emblogx/src)
else()
    FetchContent_Declare(
        ungula_core
        GIT_REPOSITORY https://github.com/alexconesap/ungula-core.git
        GIT_TAG main
    )
    FetchContent_MakeAvailable(ungula_core)
    set(LIB_CORE_SRC ${ungula_core_SOURCE_DIR}/src)

    FetchContent_Declare(
        ungula_hal
        GIT_REPOSITORY https://github.com/alexconesap/ungula-hal.git
        GIT_TAG main
    )
    FetchContent_MakeAvailable(ungula_hal)
    set(LIB_HAL_SRC ${ungula_hal_SOURCE_DIR}/src)

    FetchContent_Declare(
        emblogx
        GIT_REPOSITORY https://github.com/alexconesap/emblogx.git
        GIT_TAG main
    )
    FetchContent_MakeAvailable(emblogx)
    set(LIB_EMBLOGX_SRC ${emblogx_SOURCE_DIR}/src)
endif()

set(LIB_RTC_SRC ${CMAKE_CURRENT_LIST_DIR}/../src)

include_directories(
    ${CMAKE_CURRENT_LIST_DIR}
    ${LIB_CORE_SRC}
    ${LIB_HAL_SRC}
    ${LIB_EMBLOGX_SRC}
    ${LIB_RTC_SRC}
)

add_compile_definitions(
    EMBLOGX_BACKEND_STDIO
    EMBLOGX_LOG_PREFIX="[TEST] "
    EMBLOGX_ENABLE_SINK_CONSOLE=1
    EMBLOGX_ENABLE_SINK_MEMORY=0
    EMBLOGX_ENABLE_SINK_HTTP=0
    EMBLOGX_ENABLE_SINK_SD=0
)

# ---------------------------------------------------------------------------
# BCD codec + DateTime <-> epoch round-trip tests. Pure header-only logic;
# no HAL or emblogx dependency.
# ---------------------------------------------------------------------------
add_executable(test_ungula_rtc_codec
    test_bcd.cpp
    test_datetime_codec.cpp
)
target_link_libraries(test_ungula_rtc_codec gtest_main)

# ---------------------------------------------------------------------------
# IRtc contract + RtcTimeProvider — uses RtcFake. No I2C hardware. Catches
# interface drift via static_cast<IRtc*>(&fake) on every pure virtual.
# ---------------------------------------------------------------------------
add_executable(test_ungula_rtc
    test_rtc.cpp
    test_rtc_time_provider.cpp
    ${LIB_RTC_SRC}/ungula/rtc/i_rtc.cpp
    ${LIB_RTC_SRC}/ungula/rtc/rtc_time_provider.cpp
    ${LIB_HAL_SRC}/ungula/hal/multiplexer/i_multiplexer.cpp
    ${LIB_EMBLOGX_SRC}/emblogx/logger_core.cpp
    ${LIB_EMBLOGX_SRC}/emblogx/async_dispatcher.cpp
    ${LIB_EMBLOGX_SRC}/emblogx/sinks/console_sink.cpp
)
target_link_libraries(test_ungula_rtc gtest_main)

# ---------------------------------------------------------------------------
# Driver smoke tests against the desktop I2C stub. Verifies the drivers
# compile, link, and stay valid IRtc implementations. Full hardware path
# exercised only on the device.
# ---------------------------------------------------------------------------
add_executable(test_ungula_rtc_drivers
    test_ds3231.cpp
    test_ds1307.cpp
    ${LIB_RTC_SRC}/ungula/rtc/i_rtc.cpp
    ${LIB_RTC_SRC}/ungula/rtc/drivers/ds3231.cpp
    ${LIB_RTC_SRC}/ungula/rtc/drivers/ds1307.cpp
    ${LIB_HAL_SRC}/ungula/hal/multiplexer/i_multiplexer.cpp
    ${LIB_HAL_SRC}/ungula/hal/i2c/platforms/i2c_master_default.cpp
    ${LIB_EMBLOGX_SRC}/emblogx/logger_core.cpp
    ${LIB_EMBLOGX_SRC}/emblogx/async_dispatcher.cpp
    ${LIB_EMBLOGX_SRC}/emblogx/sinks/console_sink.cpp
)
target_link_libraries(test_ungula_rtc_drivers gtest_main)

enable_testing()
include(GoogleTest)
gtest_discover_tests(test_ungula_rtc_codec)
gtest_discover_tests(test_ungula_rtc)
gtest_discover_tests(test_ungula_rtc_drivers)
