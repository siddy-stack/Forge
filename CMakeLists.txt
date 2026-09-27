cmake_minimum_required(VERSION 3.20)

project(
    forge
    VERSION 0.1.0
    DESCRIPTION "Event-driven multiplayer server framework written in C"
    LANGUAGES C
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ============================================================
# Forge Server
# ============================================================

add_executable(forge
    src/main.c
    src/server.c
    src/network.c
    src/connection.c
    src/event_loop.c
    src/connection_manager.c
    src/client_handler.c
    src/message_dispatcher.c
    src/protocol.c
)

target_include_directories(forge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(forge PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
    )
endif()

# ============================================================
# Protocol Unit Tests
# ============================================================

add_executable(forge_protocol_test
    tests/test_protocol.c
    src/protocol.c
)

target_include_directories(forge_protocol_test
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(forge_protocol_test PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
    )
endif()

# ============================================================
# Protocol Integration Client
# ============================================================

add_executable(forge_protocol_client
    tests/protocol_client.c
    src/protocol.c
)

target_include_directories(forge_protocol_client
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(forge_protocol_client PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
    )
endif()

# ============================================================
# Sanitizers
# ============================================================

option(
    FORGE_ENABLE_SANITIZERS
    "Enable AddressSanitizer and UndefinedBehaviorSanitizer"
    OFF
)

if(FORGE_ENABLE_SANITIZERS)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")

        set(FORGE_SANITIZER_FLAGS
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )

        target_compile_options(forge PRIVATE
            ${FORGE_SANITIZER_FLAGS}
        )

        target_link_options(forge PRIVATE
            -fsanitize=address,undefined
        )

        target_compile_options(forge_protocol_test PRIVATE
            ${FORGE_SANITIZER_FLAGS}
        )

        target_link_options(forge_protocol_test PRIVATE
            -fsanitize=address,undefined
        )

        target_compile_options(forge_protocol_client PRIVATE
            ${FORGE_SANITIZER_FLAGS}
        )

        target_link_options(forge_protocol_client PRIVATE
            -fsanitize=address,undefined
        )

    endif()
endif()