cmake_minimum_required(VERSION 3.25)
project(atenciosamente VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies ─────────────────────────────────────────────────────────────
# CONFIG mode: use the package's own CrowConfig.cmake (installed by vcpkg),
# not a CMake built-in Find module. Always use CONFIG with vcpkg packages.
find_package(Crow        CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(libpqxx     CONFIG REQUIRED)
# Catch2 is in vcpkg.json (so vcpkg installs it) but find_package is added
# when tests are introduced in a later step.

# ── Executable ───────────────────────────────────────────────────────────────
add_executable(atenciosamente_server
    src/main.cpp
    src/app.cpp
    src/notification_json.cpp
    src/handlers/notifications.cpp
)

# All flags are PRIVATE: they apply only to this target, not to any consumer.
# (Nothing links against an executable anyway, but the habit matters for libs.)
target_compile_options(atenciosamente_server PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)

target_link_libraries(atenciosamente_server PRIVATE
    Crow::Crow
    nlohmann_json::nlohmann_json
    libpqxx::pqxx
)

# ── Sanitizers (dev preset sets ENABLE_SANITIZERS=ON) ───────────────────────
option(ENABLE_SANITIZERS "Enable ASan + UBSan" OFF)
if(ENABLE_SANITIZERS)
    target_compile_options(atenciosamente_server PRIVATE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer  # keeps stack traces readable under ASan
    )
    target_link_options(atenciosamente_server PRIVATE
        -fsanitize=address,undefined
    )
endif()
