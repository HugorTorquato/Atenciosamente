# Catch2 v3 — Field Guide

Quick reference for writing and running tests in this project.
Catch2 version installed: 3.14.0 (via vcpkg).

---

## The three macros you use 90% of the time

### `TEST_CASE`

Declares one test. Takes a description string and an optional tag string.

```cpp
TEST_CASE("serialize_notifications produces a JSON array", "[notification][json]")
{
    // test body
}
```

- The description is free-form text. Make it a sentence: what does it prove?
- Tags go in `[brackets]`. Multiple tags: `"[notification][json][serialization]"`.
- Tags are how you run subsets from the command line.

### `SECTION`

Divides a TEST_CASE into named sub-scenarios. Each SECTION runs independently:
Catch2 re-enters the TEST_CASE from the top before every SECTION, so any setup
code written above the first SECTION runs fresh for each one.

```cpp
TEST_CASE("my feature", "[tag]")
{
    // This runs once per SECTION — good place for shared setup.
    MyObject obj = build_test_object();

    SECTION("happy path")
    {
        REQUIRE(obj.do_thing() == expected_value);
    }

    SECTION("edge case")
    {
        REQUIRE(obj.handle_empty() == true);
    }
}
```

You don't have to use SECTIONs. A TEST_CASE with no SECTION is fine for simple cases.

### `REQUIRE` vs `CHECK`

| Macro | On failure |
|-------|-----------|
| `REQUIRE(expr)` | Stops the current SECTION immediately |
| `CHECK(expr)` | Records the failure and continues |

Rule of thumb: use `REQUIRE` when later assertions would be meaningless if this
one fails (e.g., checking `result.size() == 1` before accessing `result[0]`).
Use `CHECK` when you want to collect multiple independent failures in one run.

```cpp
REQUIRE(result.size() == 1);     // stop here if wrong — result[0] would crash
CHECK(result[0]["id"] == 42);    // collect failures; both lines are independent
CHECK(result[0]["title"] == "Hello");
```

---

## Equality and comparison matchers

```cpp
REQUIRE(value == expected);       // basic equality
REQUIRE(value != other);
REQUIRE(value > 0);
REQUIRE(value >= minimum);

// Floating point — never use == for floats
REQUIRE(value == Approx(3.14).epsilon(0.01));
```

For strings, nlohmann::json, and most types with `operator==`, plain `==` works.

---

## Checking exceptions

```cpp
// Assert that an exception IS thrown
REQUIRE_THROWS(risky_function());
REQUIRE_THROWS_AS(risky_function(), std::invalid_argument);

// Assert that no exception is thrown
REQUIRE_NOTHROW(safe_function());

// Catch and inspect the message
REQUIRE_THROWS_WITH(risky_function(), "expected error message");
```

---

## A complete test file skeleton

```cpp
#include <catch2/catch_test_macros.hpp>

// Include the production header under test — not the .cpp.
#include "my_feature.hpp"

// Optional: pull in helpers
#include <vector>
#include <string>

// File-level constants or helpers go here (not inside TEST_CASE).
static const std::string SOME_CONSTANT = "fixed value";

TEST_CASE("FeatureName does X under condition Y", "[feature][tag2]")
{
    // Shared setup (runs before every SECTION)
    SomeType obj = make_test_object();

    SECTION("normal case")
    {
        auto result = obj.process("input");
        REQUIRE(result.has_value());
        REQUIRE(result.value() == "expected");
    }

    SECTION("empty input")
    {
        auto result = obj.process("");
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("exception on null pointer")
    {
        REQUIRE_THROWS_AS(obj.process_ptr(nullptr), std::invalid_argument);
    }
}

// Multiple TEST_CASEs per file is fine. Keep them focused on one module.
TEST_CASE("FeatureName handles edge case Z", "[feature]")
{
    REQUIRE(some_free_function(0) == 0);
}
```

---

## Naming conventions in this project

| What | Convention | Example |
|------|-----------|---------|
| Test files | `<module>_test.cpp` | `notification_json_test.cpp` |
| TEST_CASE description | Sentence saying what it proves | `"serialize_notifications produces a JSON array"` |
| Tags | Module name + aspect | `[notification][json]` |
| SECTIONs | Short phrase, no period | `"empty vector produces empty array"` |

---

## Running a subset of tests

Catch2 supports two levels of filtering:

### Via CTest (coarser — matches CTest test names)

```bash
# Run all tests whose CTest name contains "notification"
ctest --preset dev -R notification

# Run tests in a specific file's cases (if names are unique enough)
ctest --preset dev -R "serialize_notifications"

# List all registered CTest tests without running them
ctest --preset dev -N
```

### Via the binary directly (finer — matches Catch2 tags or descriptions)

```bash
# Run the binary directly — gives you Catch2's own output format
./build/dev/tests/tests_unit

# Filter by tag
./build/dev/tests/tests_unit "[json]"

# Filter by description substring
./build/dev/tests/tests_unit "empty vector"

# List all TEST_CASEs without running them
./build/dev/tests/tests_unit --list-tests

# List all tags
./build/dev/tests/tests_unit --list-tags
```

---

## Understanding Catch2 output

### All tests pass

```
===============================================================================
All tests passed (12 assertions in 5 test cases)
```

### A failure

```
-------------------------------------------------------------------------------
serialize_notifications produces a JSON array
  single notification values are correct
-------------------------------------------------------------------------------
tests/unit/notification_json_test.cpp:44
...............................................................................

tests/unit/notification_json_test.cpp:49: FAILED:
  REQUIRE( obj["id"] == 42 )
with expansion:
  1 == 42

===============================================================================
test cases: 5 | 5 passed
assertions: 12 | 11 passed | 1 failed
```

Key parts:
- The TEST_CASE description, then the SECTION name — tells you exactly which path failed.
- The file and line number — click it in the IDE or go straight to it.
- **"with expansion"** — Catch2 shows the actual values, not just `false`. This is
  one of its best features: you see `1 == 42`, not just "assertion failed."

---

## Common pitfall: dangling reference from a temporary

```cpp
// WRONG — serialize_notifications() returns a temporary json value.
// [0] gives a reference INTO that temporary.
// The temporary is destroyed at the semicolon. obj is immediately dangling.
const auto& obj = serialize_notifications(notifications)[0];
REQUIRE(obj["id"] == 42);  // heap-use-after-free — ASan will catch this

// CORRECT — store the result first, then take a reference to it.
auto result = serialize_notifications(notifications);
const auto& obj = result[0];   // result is alive; reference is valid
REQUIRE(obj["id"] == 42);
```

This is a general C++ rule: never bind a reference to a sub-expression of a
temporary. The temporary dies at the statement boundary. ASan with the dev preset
will always catch it; without sanitizers it silently corrupts memory.

---

## What NOT to put in a unit test

Unit tests in `tests/unit/` test pure logic with no I/O. Don't:

- Open a database connection
- Make HTTP requests
- Read files from disk
- Sleep or depend on wall-clock time (use a fixed timestamp like `epoch{}` instead)
- Test Crow routing or handler integration (that's Phase 2: functional tests)

If a function needs a database, the right move is to separate the query from the
logic and test the logic in isolation. We'll handle that when Phase 1 (Postgres)
arrives.

---

## Three-tier test strategy

```
Tier 3: Functional tests (Phase 2)
  Run a real server, send HTTP requests, check responses.
  Slow, catch integration bugs. One per API endpoint.

Tier 2: Integration tests (Phase 1)
  Real database, no HTTP. Test repository + query logic.
  Medium speed, catch SQL and mapping bugs.

Tier 1: Unit tests  ← you are here
  Pure functions, no I/O. Instant. Run on every save.
  Catch logic bugs in serialization, validation, business rules.
```

The current test (`notification_json_test.cpp`) is Tier 1: it calls
`serialize_notifications` directly, passes in constructed structs,
and asserts on the JSON output. No server, no database, no network.
