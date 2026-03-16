cmake_minimum_required(VERSION 3.20)
project(radio-control-container VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(RCC_BUILD_TESTS     "Build unit and integration tests"              ON)
option(RCC_ENABLE_SANITIZERS "Enable Address/Undefined sanitizers (dev)"  OFF)
option(RCC_ENABLE_TSAN     "Enable ThreadSanitizer (dev only)"            OFF)
option(RCC_ENABLE_LTO      "Enable link-time optimization"                OFF)
option(RCC_ENABLE_COVERAGE "Enable coverage instrumentation"              OFF)

# CMake modules
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)

# --------------------------------------------------------------------------
# Dependencies
# --------------------------------------------------------------------------
find_package(Threads  REQUIRED)
find_package(OpenSSL  REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(YAMLCPP REQUIRED yaml-cpp)
find_package(nlohmann_json 3.11.2 REQUIRED)

if(RCC_BUILD_TESTS)
    find_package(GTest REQUIRED)
endif()

# --------------------------------------------------------------------------
# dts-common — git submodule at <repo-root>/dts-common
# CMakeLists.txt lives one level above rcc/, so ../dts-common is correct.
# --------------------------------------------------------------------------
set(DTS_COMMON_DIR "${PROJECT_SOURCE_DIR}/../dts-common"
    CACHE PATH "Path to dts-common source tree (default: git submodule)")

find_package(dts-common 1.0.0 QUIET CONFIG
    PATHS "${DTS_COMMON_DIR}/build")

if(NOT dts-common_FOUND)
    # Auto-init the submodule if not yet populated
    if(NOT EXISTS "${DTS_COMMON_DIR}/CMakeLists.txt")
        find_package(Git QUIET)
        if(Git_FOUND)
            message(STATUS "Initialising dts-common git submodule…")
            execute_process(
                COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/.."
                RESULT_VARIABLE  _submod_result
                OUTPUT_QUIET
            )
        endif()
    endif()

    if(NOT EXISTS "${DTS_COMMON_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "dts-common not found at ${DTS_COMMON_DIR}\n"
            "Run:  git submodule update --init --recursive\n"
            "Or override with:  -DDTS_COMMON_DIR=<path>")
    endif()

    message(STATUS "dts-common: building from submodule at ${DTS_COMMON_DIR}")
    add_subdirectory(${DTS_COMMON_DIR}
                     ${CMAKE_BINARY_DIR}/dts-common
                     EXCLUDE_FROM_ALL)
else()
    message(STATUS "dts-common: using installed package at ${dts-common_DIR}")
endif()

# --------------------------------------------------------------------------
# Version stamp (generated into build tree, never committed)
# --------------------------------------------------------------------------
execute_process(
    COMMAND git describe --tags --always --dirty
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE  RCC_GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT RCC_GIT_VERSION)
    set(RCC_GIT_VERSION "unknown")
endif()
string(TIMESTAMP RCC_BUILD_TIME "%Y-%m-%dT%H:%M:%SZ" UTC)

configure_file(
    "${PROJECT_SOURCE_DIR}/include/rcc/version.hpp.in"
    "${PROJECT_BINARY_DIR}/generated/rcc/version.hpp"
    @ONLY
)
include_directories(${PROJECT_BINARY_DIR}/generated)

# --------------------------------------------------------------------------
# Coverage instrumentation (mirrors gpsd-proxy pattern)
# --------------------------------------------------------------------------
if(RCC_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
        add_link_options(-fprofile-instr-generate)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(--coverage)
        add_link_options(--coverage)
    else()
        message(WARNING "Coverage not supported for ${CMAKE_CXX_COMPILER_ID}")
    endif()
endif()

# --------------------------------------------------------------------------
# Build targets
# --------------------------------------------------------------------------
add_subdirectory(src)

if(RCC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# --------------------------------------------------------------------------
# Install
# --------------------------------------------------------------------------
install(TARGETS radio-control-container RUNTIME DESTINATION bin)
install(DIRECTORY config/ DESTINATION etc/rcc FILES_MATCHING PATTERN "*.yaml")

# --------------------------------------------------------------------------
# Summary
# --------------------------------------------------------------------------
message(STATUS "")
message(STATUS "Radio Control Container (C++20)")
message(STATUS "  Version:      ${PROJECT_VERSION}  (${RCC_GIT_VERSION})")
message(STATUS "  Compiler:     ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Build type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  Tests:        ${RCC_BUILD_TESTS}")
message(STATUS "  Sanitizers:   ${RCC_ENABLE_SANITIZERS}")
message(STATUS "  TSAN:         ${RCC_ENABLE_TSAN}")
message(STATUS "  LTO:          ${RCC_ENABLE_LTO}")
message(STATUS "  Coverage:     ${RCC_ENABLE_COVERAGE}")
message(STATUS "")
