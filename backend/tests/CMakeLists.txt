find_package(Catch2 CONFIG REQUIRED)

add_executable(tests_unit
    unit/notification_json_test.cpp
    unit/create_notification_request_test.cpp
)

# Catch2::Catch2WithMain provides a main() so we don't write one.
# atenciosamente_core brings in: notification_json.cpp, nlohmann_json, src/ includes.
target_link_libraries(tests_unit PRIVATE
    atenciosamente_core
    Catch2::Catch2WithMain
)

# Tests must be compiled AND linked with the same sanitizer flags as production
# code. A mismatch causes ASan/UBSan to abort on startup.
if(ENABLE_SANITIZERS)
    target_compile_options(tests_unit PRIVATE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(tests_unit PRIVATE
        -fsanitize=address,undefined
    )
endif()

# include(Catch) loads catch_discover_tests() from Catch2's CMake install.
# catch_discover_tests probes the binary with --list-tests at build time and
# registers each TEST_CASE as a separate CTest entry — so failures are reported
# per test case, not per executable.
include(Catch)
catch_discover_tests(tests_unit)
