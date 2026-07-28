---
name: backend-add-test
description: >-
  Add a Catch2 test to the Atenciosamente C++ backend — unit or integration,
  whichever tier the code under test belongs to. Use when new domain/
  repository logic lacks coverage, or when asked to test a specific function.
  Covers tier selection, file placement, CMakeLists.txt wiring, and the
  per-test transaction-rollback isolation pattern for integration tests.
---

# Add a backend test

Two tiers exist today, each its own CTest-discovered binary in `backend/tests/CMakeLists.txt`,
each its own CI job (`.github/workflows/backend-ci.yml`). Pick the tier by asking **what the
code under test touches**, not by how important the code feels — see the table below.

## 1. Pick the tier

| Layer | Touches | Tier | Binary | CTest prefix |
|---|---|---|---|---|
| `src/domain/` | Nothing — pure data, validation, (de)serialization | **Unit** | `tests_unit` | `unit/` |
| `src/repository/` | Postgres, via `pqxx::work&` | **Integration** | `tests_integration` | `integration/` |
| `src/handlers/`, `src/app.cpp` | Crow HTTP + everything below | **Functional/E2E** | — not built yet (Phase 2+, see `PROJECT_PLAN.md` §8) | — |
| `src/db/connection.{hpp,cpp}` | Postgres (just opening it) | — no dedicated test | Exercised as a side effect of every integration test's `make_connection()` call; there's no branching logic in it worth asserting on alone | — |

Rule of thumb: **the lowest tier that can still catch the bug you're worried about.** Pure
logic needs no database — don't reach for `pqxx::work` to test a validation rule. Anything
that runs SQL can only be proven correct against a real Postgres — a mock would just test
your assumptions about Postgres, not Postgres itself.

If the code under test doesn't cleanly land in `domain/` or `repository/` (e.g. it's new
logic in `handlers/`), there is no tier for it yet — flag that instead of forcing a fit; see
§4.

## 2. Unit test procedure

1. File: `backend/tests/unit/<subject>_test.cpp`. Style reference:
   `notification_json_test.cpp` (serialization) and `create_notification_request_test.cpp`
   (validation) — `TEST_CASE`/`SECTION` naming, assertion style, comment density.
2. No `pqxx`, no `make_connection()`, no `crow.h` — if you find yourself reaching for any of
   those, the code under test isn't actually domain logic; re-check step 1's table.
3. Add the new file to `add_executable(tests_unit ...)`'s source list in
   `backend/tests/CMakeLists.txt`. `catch_discover_tests(tests_unit)` already applies
   `TEST_PREFIX "unit/"` — nothing else to wire.
4. Run: `cmake --build --preset=dev && ctest --preset=dev -R '^unit/'` (no database needed).

## 3. Integration test procedure

1. File: `backend/tests/integration/<subject>_test.cpp`. Style reference:
   `notification_repository_test.cpp`.
2. **Isolation pattern — the one rule that matters:** each `TEST_CASE` opens its own
   `pqxx::connection conn = make_connection();` and `pqxx::work txn{conn};`, does its
   inserts/queries through `txn`, asserts, and **never calls `txn.commit()`**. When `txn`
   goes out of scope (normal return or a failed `REQUIRE` unwinding the stack), libpqxx's
   destructor sends `ROLLBACK` — nothing this test wrote is ever visible outside it, and no
   `DELETE`/`TRUNCATE` cleanup step is needed. Don't add one.
3. Add the new file to `add_executable(tests_integration ...)`'s source list in
   `backend/tests/CMakeLists.txt`. `TEST_PREFIX "integration/"` is already applied.
4. Run locally: `docker compose up -d db`, apply migrations if the schema changed
   (`backend/scripts/migrate.sh`), then
   `cmake --build --preset=dev && ctest --preset=dev -R '^integration/'`.
5. Re-run it a second time and confirm it still passes — that's the concrete check that
   rollback actually left no rows behind, not just that the assertions happened to hold once.

## 4. No tier fits (handlers/, app.cpp)

Functional/E2E tests (real HTTP against a running server + real Postgres) are the tier that
covers `handlers/`/`app.cpp`, and they're deliberately not built yet — Phase 2+ per
`PROJECT_PLAN.md` §8. Don't invent a workaround (e.g. calling a handler function directly
with a hand-built `crow::request`) to force coverage early; that tests something narrower
than the thing you actually care about (the HTTP wiring) and creates a false sense of
coverage. Flag the gap instead — see `backend.md`'s test-coverage note — and let it wait for
the real tier.

## 5. Commit

One line, `Backend (Test): <what you covered>` (no body, no trailers) — e.g.
`Backend (Test): add integration test for insert() duplicate-title handling`. If the test
exposed a bug, fix it in a **separate** commit from the test that caught it, so the test's
own commit shows it passing against corrected behavior — matches this repo's "one focused
task per conversation" rhythm even within a single test-adding task.
