# Copyright (c) 2024 Devexperts LLC.
# SPDX-License-Identifier: MPL-2.0

cmake_minimum_required(VERSION 3.10)
project(SUPDXFD_17799)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(CMAKE_CXX_STANDARD 11)

include(FetchContent)

set(FETCHCONTENT_QUIET FALSE)

set(TARGET_PLATFORM "x64")
set(DISABLE_TLS ON)
set(DXFC_BUILD_INTEGRATION_TESTS OFF)

FetchContent_Declare(
        DXFeed
        GIT_REPOSITORY "https://github.com/dxFeed/dxfeed-c-api.git" GIT_TAG master
        USES_TERMINAL_DOWNLOAD TRUE
        GIT_PROGRESS TRUE
        DOWNLOAD_NO_EXTRACT FALSE
)

FetchContent_MakeAvailable(DXFeed)

set(FMTLIB_VERSION "10.2.1")
set(FMT_INSTALL OFF)

FetchContent_Declare(
        fmt
        URL https://github.com/fmtlib/fmt/releases/download/${FMTLIB_VERSION}/fmt-${FMTLIB_VERSION}.zip # downloading a zip release speeds up the download
        USES_TERMINAL_DOWNLOAD TRUE
        GIT_PROGRESS TRUE
        DOWNLOAD_NO_EXTRACT FALSE
)
FetchContent_MakeAvailable(fmt)

add_executable(${PROJECT_NAME} main.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC ${DXFeed_SOURCE_DIR}/../include)
target_compile_definitions(${PROJECT_NAME} PRIVATE FMT_HEADER_ONLY=1 _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS=1)
target_link_libraries(${PROJECT_NAME} PUBLIC DXFeed fmt::fmt-header-only)
