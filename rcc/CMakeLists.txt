cmake_minimum_required(VERSION 3.20)
project(radio-control-container-cpp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(RCC_BUILD_TESTS "Build unit and integration tests" ON)
option(RCC_ENABLE_SANITIZERS "Enable Address/Undefined sanitizers (dev only)" OFF)
option(RCC_ENABLE_TSAN "Enable ThreadSanitizer (dev only)" OFF)
option(RCC_ENABLE_LTO "Enable link-time optimization" OFF)

# CMake modules
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)

# Dependencies
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(YAMLCPP REQUIRED yaml-cpp)
find_package(nlohmann_json 3.11.2 REQUIRED)
find_package(fmt REQUIRED)
find_package(Catch2 3 REQUIRED)

# dts-common (find installed package first, fallback to submodule)
find_package(dts-common 1.0.0 QUIET CONFIG PATHS "${PROJECT_SOURCE_DIR}/../dts-common/build")

if(NOT dts-common_FOUND)
    message(STATUS "dts-common not found as installed package, building from submodule")

    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/../dts-common/CMakeLists.txt")
        message(FATAL_ERROR "dts-common submodule missing. Run: git submodule update --init --recursive")
    endif()

    add_subdirectory(${PROJECT_SOURCE_DIR}/../dts-common ${CMAKE_BINARY_DIR}/dts-common EXCLUDE_FROM_ALL)
else()
    message(STATUS "Found dts-common: ${dts-common_DIR}")
endif()

# Version information
execute_process(
    COMMAND git describe --tags --always --dirty
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE RCC_GIT_VERSION
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

# Subdirectories
add_subdirectory(src)

if(RCC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# Installation rules
install(TARGETS radio-control-container
    RUNTIME DESTINATION bin
)

install(DIRECTORY config/
    DESTINATION etc/rcc
    FILES_MATCHING PATTERN "*.yaml"
)

# Configuration summary
message(STATUS "")
message(STATUS "Radio Control Container (C++20) Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Git Version: ${RCC_GIT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Build Tests: ${RCC_BUILD_TESTS}")
message(STATUS "  Sanitizers: ${RCC_ENABLE_SANITIZERS}")
message(STATUS "  ThreadSanitizer: ${RCC_ENABLE_TSAN}")
message(STATUS "  LTO: ${RCC_ENABLE_LTO}")
message(STATUS "")

cmake_minimum_required(VERSION 3.20)
project(radio-control-container VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(RCC_BUILD_TESTS "Build unit and integration tests" ON)
option(RCC_ENABLE_SANITIZERS "Enable Address/Undefined sanitizers (dev only)" OFF)
option(RCC_ENABLE_TSAN "Enable ThreadSanitizer (dev only)" OFF)
option(RCC_ENABLE_LTO "Enable link-time optimization" OFF)

# CMake modules
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)

# Dependencies
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(YAMLCPP REQUIRED yaml-cpp)
find_package(nlohmann_json 3.11.2 REQUIRED)
find_package(fmt REQUIRED)

# dts-common (find installed package first, fallback to submodule)
find_package(dts-common 1.0.0 QUIET CONFIG PATHS "${PROJECT_SOURCE_DIR}/dts-common/build")

if(NOT dts-common_FOUND)
    message(STATUS "dts-common not found as installed package, building from submodule")

    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/dts-common/CMakeLists.txt")
        message(FATAL_ERROR "dts-common submodule missing. Run: git submodule update --init --recursive")
    endif()

    add_subdirectory(dts-common EXCLUDE_FROM_ALL)
else()
    message(STATUS "Found dts-common: ${dts-common_DIR}")
endif()

# Version information
execute_process(
    COMMAND git describe --tags --always --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE RCC_GIT_VERSION
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

# Subdirectories
add_subdirectory(src)

if(RCC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# Installation rules
install(TARGETS radio-control-container
    RUNTIME DESTINATION bin
)

install(DIRECTORY config/
    DESTINATION etc/rcc
    FILES_MATCHING PATTERN "*.yaml"
)

# Configuration summary
message(STATUS "")
message(STATUS "Radio Control Container Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Git Version: ${RCC_GIT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Build Tests: ${RCC_BUILD_TESTS}")
message(STATUS "  Sanitizers: ${RCC_ENABLE_SANITIZERS}")
message(STATUS "  ThreadSanitizer: ${RCC_ENABLE_TSAN}")
message(STATUS "  LTO: ${RCC_ENABLE_LTO}")
message(STATUS "")


