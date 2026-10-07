cmake_minimum_required(VERSION 3.25)
project(atenciosamente VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies ──────────────────────────────────────────────────────────────
# CONFIG mode: use the package's own CrowConfig.cmake (installed by vcpkg),
# not a CMake built-in Find module. Always use CONFIG with vcpkg packages.
find_package(Crow          CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(libpqxx       CONFIG REQUIRED)

# ── Core library ──────────────────────────────────────────────────────────────
# STATIC: produces libatenciosamente_core.a, linked by both the server and
# tests. Avoids compiling the same .cpp files twice and keeps source lists
# in one place.
#
# PUBLIC include directories: any target that links this library automatically
# gets src/ on its include search path. This lets tests/unit/*.cpp write
# #include "notification_json.hpp" without knowing where src/ is.
#
# PUBLIC nlohmann_json / libpqxx / Threads: the server and test executables
# inherit these dependencies transitively — they do not need to repeat them.
# libpqxx is PUBLIC (not PRIVATE) because atenciosamente_core's public
# headers now include <pqxx/connection> (see src/db/connection.hpp) — any
# consumer that #includes that header needs libpqxx's include path and,
# once it links, its symbols too. The upcoming tests_integration target
# (S5) will construct connections through this same module, so it needs
# that same libpqxx dependency to flow through automatically. Threads is
# PUBLIC for the same reason: src/db/connection_pool.hpp is a public header
# that includes <mutex>/<condition_variable>, so every consumer needs
# pthread's symbols linked in too. Declared explicitly here rather than
# relying on it arriving transitively via Crow (which happens to pull in
# pthread through Asio) — tests_unit/tests_integration never link Crow at
# all, and an implicit, undeclared dependency that only "works" because the
# dev container's glibc folds pthread into libc is exactly the kind of
# fragile assumption this project avoids.
find_package(Threads REQUIRED)

add_library(atenciosamente_core STATIC
    src/domain/notification_json.cpp
    src/domain/create_notification_request.cpp
    src/db/connection.cpp
    src/db/connection_pool.cpp
    src/repository/notification_repository.cpp
)

target_include_directories(atenciosamente_core PUBLIC
    "${CMAKE_SOURCE_DIR}/src"
)

target_compile_options(atenciosamente_core PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)

target_link_libraries(atenciosamente_core PUBLIC
    nlohmann_json::nlohmann_json
    libpqxx::pqxx
    Threads::Threads
)

# ── Server executable ─────────────────────────────────────────────────────────
add_executable(atenciosamente_server
    src/main.cpp
    src/app.cpp
    src/handlers/notifications.cpp
)

target_compile_options(atenciosamente_server PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)

# nlohmann_json, libpqxx, and the src/ include path come transitively from
# atenciosamente_core — no need to repeat them here.
target_link_libraries(atenciosamente_server PRIVATE
    atenciosamente_core
    Crow::Crow
)

# ── Sanitizers (dev preset sets ENABLE_SANITIZERS=ON) ────────────────────────
option(ENABLE_SANITIZERS "Enable ASan + UBSan" OFF)
if(ENABLE_SANITIZERS)
    # Both targets must be compiled with sanitizer instrumentation.
    # Link flags go only on executables — static libs have no linker step.
    foreach(_target atenciosamente_core atenciosamente_server)
        target_compile_options(${_target} PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
    endforeach()
    target_link_options(atenciosamente_server PRIVATE
        -fsanitize=address,undefined
    )
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
# include(CTest) does two things:
#   1. Calls enable_testing() so CTest knows where the test tree lives.
#   2. Defines the BUILD_TESTING cache variable (default ON).
# We guard add_subdirectory so a -DBUILD_TESTING=OFF production build never
# touches test source or links Catch2.
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
