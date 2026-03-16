# Sanitizers.cmake
# AddressSanitizer, UndefinedBehaviorSanitizer, and ThreadSanitizer helpers

function(enable_sanitizers target_name)
    if(RCC_ENABLE_SANITIZERS)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
            message(STATUS "Enabling sanitizers for ${target_name}")

            set(SANITIZER_FLAGS
                -fsanitize=address
                -fsanitize=undefined
                -fno-omit-frame-pointer
                -fno-optimize-sibling-calls
            )

            target_compile_options(${target_name} PRIVATE ${SANITIZER_FLAGS})
            target_link_options(${target_name} PRIVATE ${SANITIZER_FLAGS})
        else()
            message(WARNING "Sanitizers not supported for compiler ${CMAKE_CXX_COMPILER_ID}")
        endif()
    endif()
endfunction()

function(enable_thread_sanitizer target_name)
    if(RCC_ENABLE_TSAN)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
            message(STATUS "Enabling ThreadSanitizer for ${target_name}")

            set(TSAN_FLAGS
                -fsanitize=thread
                -fno-omit-frame-pointer
            )

            target_compile_options(${target_name} PRIVATE ${TSAN_FLAGS})
            target_link_options(${target_name} PRIVATE ${TSAN_FLAGS})
        else()
            message(WARNING "ThreadSanitizer not supported for compiler ${CMAKE_CXX_COMPILER_ID}")
        endif()
    endif()
endfunction()

# Sanitizers.cmake
# AddressSanitizer, UndefinedBehaviorSanitizer, and optional ThreadSanitizer

function(enable_sanitizers target_name)
    if(RCC_ENABLE_SANITIZERS)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
            message(STATUS "Enabling sanitizers for ${target_name}")

            set(SANITIZER_FLAGS
                -fsanitize=address
                -fsanitize=undefined
                -fno-omit-frame-pointer
                -fno-optimize-sibling-calls
            )

            target_compile_options(${target_name} PRIVATE ${SANITIZER_FLAGS})
            target_link_options(${target_name} PRIVATE ${SANITIZER_FLAGS})
        else()
            message(WARNING "Sanitizers not supported for compiler ${CMAKE_CXX_COMPILER_ID}")
        endif()
    endif()
endfunction()

function(enable_thread_sanitizer target_name)
    if(RCC_ENABLE_TSAN)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
            message(STATUS "Enabling ThreadSanitizer for ${target_name}")

            set(TSAN_FLAGS
                -fsanitize=thread
                -fno-omit-frame-pointer
            )

            target_compile_options(${target_name} PRIVATE ${TSAN_FLAGS})
            target_link_options(${target_name} PRIVATE ${TSAN_FLAGS})
        else()
            message(WARNING "ThreadSanitizer not supported for compiler ${CMAKE_CXX_COMPILER_ID}")
        endif()
    endif()
endfunction()


