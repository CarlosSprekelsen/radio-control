cmake_minimum_required(VERSION 3.16)
project(silvus-mock LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(Threads REQUIRED)

# dts-common assumed to be available as a target or include path
include_directories(${CMAKE_SOURCE_DIR}/../../../libs/dts-common/include)

add_executable(silvus-mock
    src/main.cpp
    src/http_server.cpp
    src/http_server.hpp
    src/maintenance_server.cpp
    src/maintenance_server.hpp
    src/silvus_mock.cpp
    src/silvus_mock.hpp
    src/web_assets.cpp
    src/web_assets.hpp
)

target_link_libraries(silvus-mock PRIVATE Threads::Threads)
