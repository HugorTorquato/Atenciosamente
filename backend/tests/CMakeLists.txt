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
# TEST_PREFIX namespaces every discovered CTest name under "unit/" so CI can
# select this binary's tests with `ctest -R '^unit/'` without touching
# tests_integration's — see the CMakeLists comment above tests_integration and
# .github/workflows/backend-ci.yml for the other half of this.
catch_discover_tests(tests_unit TEST_PREFIX "unit/")

# ── Integration tests ─────────────────────────────────────────────────────────
# Separate executable from tests_unit, not more source files added to it:
# these TEST_CASEs open a real connection to Postgres (via make_connection())
# and need POSTGRES_* env vars and a live `db` service to pass, unlike
# tests_unit which never touches a database. Keeping them in their own binary
# means `ctest -R tests_unit` (or CI, before Postgres is relevant) can run
# without a database at all.
add_executable(tests_integration
    integration/notification_repository_test.cpp
)

# atenciosamente_core brings in: notification_repository.cpp, db/connection.cpp,
# libpqxx (PUBLIC on atenciosamente_core), and the src/ include path.
# libpqxx::pqxx is linked explicitly here anyway, matching this project's style
# of not relying solely on transitive PUBLIC propagation — even though
# atenciosamente_core already brings it in via db/connection.hpp.
target_link_libraries(tests_integration PRIVATE
    atenciosamente_core
    libpqxx::pqxx
    Catch2::Catch2WithMain
)

if(ENABLE_SANITIZERS)
    target_compile_options(tests_integration PRIVATE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(tests_integration PRIVATE
        -fsanitize=address,undefined
    )
endif()

# Mirrors tests_unit's TEST_PREFIX so CI can select only this binary's tests
# with `ctest -R '^integration/'`.
catch_discover_tests(tests_integration TEST_PREFIX "integration/")
