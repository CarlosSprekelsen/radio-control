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
set(DTS_COMMON_DIR "" CACHE PATH "Legacy alias for DTS_COMMON_SOURCE_DIR")
set(DTS_COMMON_SOURCE_DIR "" CACHE PATH "Path to dts-common source tree")
set(DTS_COMMON_PACKAGE_DIR "" CACHE PATH "Path to dts-common install prefix")

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
# dts-common — prefer the shared checkout at ../../dts-common, then fall back
# to the embedded submodule at ../dts-common.
# --------------------------------------------------------------------------
if(DTS_COMMON_DIR AND NOT DTS_COMMON_SOURCE_DIR)
    set(DTS_COMMON_SOURCE_DIR "${DTS_COMMON_DIR}")
endif()

set(_dts_common_source_dir "")
if(DTS_COMMON_SOURCE_DIR)
    file(TO_CMAKE_PATH "${DTS_COMMON_SOURCE_DIR}" _dts_common_source_dir)
endif()

set(_dts_common_package_dir "")
if(DTS_COMMON_PACKAGE_DIR)
    file(TO_CMAKE_PATH "${DTS_COMMON_PACKAGE_DIR}" _dts_common_package_dir)
    list(PREPEND CMAKE_PREFIX_PATH "${_dts_common_package_dir}")
endif()

get_filename_component(_dts_common_shared_dir
    "${PROJECT_SOURCE_DIR}/../../dts-common"
    ABSOLUTE
)
file(TO_CMAKE_PATH "${_dts_common_shared_dir}" _dts_common_shared_dir)

get_filename_component(_dts_common_submodule_dir
    "${PROJECT_SOURCE_DIR}/../dts-common"
    ABSOLUTE
)
file(TO_CMAKE_PATH "${_dts_common_submodule_dir}" _dts_common_submodule_dir)

find_package(dts-common 1.0.0 QUIET CONFIG)

if(dts-common_FOUND)
    message(STATUS "dts-common: using installed package at ${dts-common_DIR}")
elseif(_dts_common_source_dir)
    if(NOT EXISTS "${_dts_common_source_dir}/CMakeLists.txt")
        message(FATAL_ERROR
            "DTS_COMMON_SOURCE_DIR='${_dts_common_source_dir}' is invalid.\n"
            "Expected a directory containing CMakeLists.txt")
    endif()
    message(STATUS "dts-common: using explicit source dir ${_dts_common_source_dir}")
    add_subdirectory("${_dts_common_source_dir}"
                     "${CMAKE_BINARY_DIR}/dts-common"
                     EXCLUDE_FROM_ALL)
elseif(EXISTS "${_dts_common_shared_dir}/CMakeLists.txt")
    message(STATUS "dts-common: using shared source dir ${_dts_common_shared_dir}")
    add_subdirectory("${_dts_common_shared_dir}"
                     "${CMAKE_BINARY_DIR}/dts-common"
                     EXCLUDE_FROM_ALL)
elseif(EXISTS "${_dts_common_submodule_dir}/CMakeLists.txt")
    message(STATUS "dts-common: building from submodule at ${_dts_common_submodule_dir}")
    add_subdirectory("${_dts_common_submodule_dir}"
                     "${CMAKE_BINARY_DIR}/dts-common"
                     EXCLUDE_FROM_ALL)
else()
    message(FATAL_ERROR
        "dts-common was not found.\n"
        "Provide one of:\n"
        "  -DDTS_COMMON_PACKAGE_DIR=/path/to/dts-common/install\n"
        "  -DDTS_COMMON_SOURCE_DIR=/path/to/dts-common/source\n"
        "Or initialize the submodule with:\n"
        "  git submodule update --init --recursive")
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
