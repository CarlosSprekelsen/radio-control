set(RCC_LIBRARY_SOURCES
    core/application.cpp
    config/config_manager.cpp
)

add_library(rcc-core ${RCC_LIBRARY_SOURCES})

target_include_directories(rcc-core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_BINARY_DIR}/generated
    PRIVATE
        ${YAMLCPP_INCLUDE_DIRS}
)

target_compile_definitions(rcc-core
    PRIVATE
        DTS_INLINE_NAMESPACE=v1
        ASIO_STANDALONE
)

target_link_libraries(rcc-core
    PUBLIC
        dts::common::core
    PRIVATE
        dts::common::net
        Threads::Threads
        OpenSSL::SSL
        OpenSSL::Crypto
        nlohmann_json::nlohmann_json
        fmt::fmt
        ${YAMLCPP_LIBRARIES}
)

set_project_warnings(rcc-core)
enable_sanitizers(rcc-core)
enable_thread_sanitizer(rcc-core)

if(RCC_ENABLE_LTO AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set_target_properties(rcc-core PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

add_executable(radio-control-container
    core/main.cpp
)
target_link_libraries(radio-control-container PRIVATE rcc-core)
set_project_warnings(radio-control-container)
enable_sanitizers(radio-control-container)
enable_thread_sanitizer(radio-control-container)

target_compile_definitions(radio-control-container
    PRIVATE
        DTS_INLINE_NAMESPACE=v1
        ASIO_STANDALONE
)


