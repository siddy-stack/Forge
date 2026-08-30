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

add_executable(forge
    src/main.c
    src/server.c
    src/network.c
    src/connection.c
    src/event_loop.c
    src/connection_manager.c
    src/client_handler.c
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
