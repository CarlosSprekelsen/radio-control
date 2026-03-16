if(NOT RCC_BUILD_TESTS)
    return()
endif()

add_executable(rcc-smoke-test smoke_test.cpp)
target_link_libraries(rcc-smoke-test PRIVATE rcc-core Catch2::Catch2WithMain)
target_include_directories(rcc-smoke-test PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_BINARY_DIR}/generated)
set_project_warnings(rcc-smoke-test)
enable_sanitizers(rcc-smoke-test)
enable_thread_sanitizer(rcc-smoke-test)

add_test(NAME rcc-smoke COMMAND rcc-smoke-test)

message(STATUS "RCC tests scaffolding placeholder - tests will be added in subsequent tasks")


