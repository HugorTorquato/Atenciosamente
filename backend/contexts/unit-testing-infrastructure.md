# Unit Testing Infrastructure

How the test build is wired together in this project.

---

## Directory layout

```
/workspace
├── CMakeLists.txt              ← top-level; adds tests/ subdirectory
├── CMakePresets.json           ← dev/ci presets; test presets live here too
├── src/
│   ├── notification.hpp        ← plain data struct (no logic to test)
│   ├── notification_json.hpp
│   └── notification_json.cpp   ← compiled into atenciosamente_core (STATIC lib)
└── tests/
    ├── CMakeLists.txt          ← declares tests_unit executable
    └── unit/
        └── notification_json_test.cpp
```

---

## The shared library: `atenciosamente_core`

The server executable and the test executable both need `notification_json.cpp`.
Rather than compiling it twice, it lives in a STATIC library target.

```
atenciosamente_core (STATIC)
  sources : src/notification_json.cpp
  PUBLIC includes : src/             ← consumers get this include path for free
  PUBLIC links    : nlohmann_json    ← consumers inherit this too
```

Anything that does `target_link_libraries(... atenciosamente_core)` automatically
gets the `src/` include path and `nlohmann_json`. That's why test files can write
`#include "notification_json.hpp"` without needing to know the full path.

When you add a new business-logic `.cpp` file that tests will need, add it to
`atenciosamente_core` in the top-level `CMakeLists.txt`.

---

## CMake target graph

```
atenciosamente_server  ──PRIVATE──▶  atenciosamente_core
                        ──PRIVATE──▶  Crow::Crow
                        ──PRIVATE──▶  libpqxx::pqxx

tests_unit             ──PRIVATE──▶  atenciosamente_core
                        ──PRIVATE──▶  Catch2::Catch2WithMain
```

`PRIVATE` on an executable always means "I need this to build and run, but I'm
not a library so no one else needs to know about it."

---

## Sanitizers

The dev preset sets `ENABLE_SANITIZERS=ON`. When that flag is on:

- `atenciosamente_core` is compiled with `-fsanitize=address,undefined`
- `atenciosamente_server` is compiled and linked with the same flags
- `tests_unit` is compiled and linked with the same flags (in `tests/CMakeLists.txt`)

A target that is linked with a sanitizer-instrumented library **must** also be
compiled with the same flags. A mismatch causes ASan to abort before `main()`.

---

## BUILD_TESTING guard

`include(CTest)` in the top-level `CMakeLists.txt` defines the `BUILD_TESTING`
cache variable (default `ON`) and calls `enable_testing()`.

```cmake
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
```

Production build without tests:

```bash
cmake --preset ci -DBUILD_TESTING=OFF
```

---

## `catch_discover_tests` vs `add_test`

| | `add_test` | `catch_discover_tests` |
|---|---|---|
| CTest sees | 1 test (the whole binary) | 1 test per TEST_CASE |
| On failure | "binary failed" | "which case failed" |
| Re-run one case | no | yes |
| Parallel (`ctest -j4`) | binary-level only | case-level |

We always use `catch_discover_tests`. It probes the binary with `--list-tests`
at build time and generates the CTest entries automatically.

---

## Running tests

```bash
# Configure + build (first time or after CMake changes)
cmake --preset dev
cmake --build --preset dev

# Run all tests
ctest --preset dev

# Re-run only failed tests
ctest --preset dev --rerun-failed

# Run tests matching a pattern (by CTest test name)
ctest --preset dev -R notification

# Run tests matching a Catch2 tag (run the binary directly)
./build/dev/tests/tests_unit "[json]"

# Verbose output (always, not just on failure)
ctest --preset dev -V
```

---

## Adding a new source file to the core library

1. Create `src/my_feature.hpp` and `src/my_feature.cpp`.
2. Add `src/my_feature.cpp` to the `atenciosamente_core` sources in `CMakeLists.txt`.
3. Create `tests/unit/my_feature_test.cpp`.
4. Add `tests/unit/my_feature_test.cpp` to `tests_unit` in `tests/CMakeLists.txt`.
5. Rebuild and run tests.

Step 4 is the only non-obvious one: `tests_unit` lists its source files explicitly,
so new test files must be added there manually.
