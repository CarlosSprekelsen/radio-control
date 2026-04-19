# Test infrastructure for radio-control-container
# Mirrors the pattern used in gpsd-Container-proxy for a consistent DTS
# learning curve across projects.

# Support library (shared fakes + utilities)
add_library(rcc-test-support STATIC
    support/fake_radio_server.cpp
    support/test_globals.cpp
)

target_include_directories(rcc-test-support
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_BINARY_DIR}/generated
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(rcc-test-support
    PUBLIC
        rcc-core
        Threads::Threads
)

target_compile_definitions(rcc-test-support
    PRIVATE DTS_INLINE_NAMESPACE=v1 ASIO_STANDALONE
)
enable_sanitizers(rcc-test-support)
enable_thread_sanitizer(rcc-test-support)

# Unit tests
file(GLOB_RECURSE UNIT_TEST_SOURCES "unit/*.cpp")

foreach(test_file ${UNIT_TEST_SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name}
        PRIVATE rcc-core rcc-test-support GTest::gtest GTest::gtest_main Threads::Threads
    )
    target_compile_definitions(${test_name} PRIVATE DTS_INLINE_NAMESPACE=v1 ASIO_STANDALONE)
    target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    enable_sanitizers(${test_name})
    enable_thread_sanitizer(${test_name})
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# Integration tests
file(GLOB_RECURSE INTEGRATION_TEST_SOURCES "integration/*.cpp")

foreach(test_file ${INTEGRATION_TEST_SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name}
        PRIVATE rcc-core rcc-test-support GTest::gtest GTest::gtest_main Threads::Threads
    )
    target_compile_definitions(${test_name} PRIVATE DTS_INLINE_NAMESPACE=v1 ASIO_STANDALONE)
    target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    enable_sanitizers(${test_name})
    enable_thread_sanitizer(${test_name})
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
