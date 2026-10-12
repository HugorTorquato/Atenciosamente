---
name: backend-add-test
description: >-
  Add a test to the Atenciosamente backend — a Catch2 unit or integration test,
  or a Python/pytest functional (black-box HTTP) test, whichever tier the code
  under test belongs to. Use when new domain/repository/handler logic lacks
  coverage, or when asked to test a specific function or endpoint. Covers tier
  selection, file placement, CMakeLists.txt wiring, the per-test
  transaction-rollback isolation pattern for integration tests, and the
  manual-only functional suite driven by scripts/functional.sh.
---

# Add a backend test

Three tiers exist. The two C++ ones are CTest-discovered binaries defined in
`backend/tests/CMakeLists.txt`, each with its own CI job
(`.github/workflows/backend-ci.yml`). The third is a Python/pytest suite that is **not** a
build target, **not** in CTest, and **has no CI job** — see §4. Pick the tier by asking
**what the code under test touches**, not by how important the code feels.

## 1. Pick the tier

| Layer | Touches | Tier | Binary | CTest prefix |
|---|---|---|---|---|
| `src/domain/` | Nothing — pure data, validation, (de)serialization | **Unit** | `tests_unit` | `unit/` |
| `src/repository/` | Postgres, via `pqxx::work&` | **Integration** | `tests_integration` | `integration/` |
| `src/handlers/`, `src/app.cpp` | Crow HTTP + everything below | **Functional** | — Python/pytest, not a build target; run via `scripts/dev.sh functional` (§4) | — not in CTest |
| `src/db/connection.{hpp,cpp}` | Postgres (just opening it) | — no dedicated test | Exercised as a side effect of every integration test's `make_connection()` call; there's no branching logic in it worth asserting on alone | — |

Rule of thumb: **the lowest tier that can still catch the bug you're worried about.** Pure
logic needs no database — don't reach for `pqxx::work` to test a validation rule. Anything
that runs SQL can only be proven correct against a real Postgres — a mock would just test
your assumptions about Postgres, not Postgres itself.

The functional tier is the **last** resort, not the convenient one. It is the slowest, the
only one that needs a running server, the only one that destroys your dev data, and the
only one with no CI safety net — so a bug provable in `domain/` must be tested there, even
when the same bug is also visible over HTTP. Reach for §4 only when the thing you need to
prove *is* the HTTP wiring: a status code, a header, route registration, or a property that
only holds end to end (e.g. "a rejected request writes no row").

## 2. Unit test procedure

1. File: `backend/tests/unit/<subject>_test.cpp`. Style reference:
   `notification_json_test.cpp` (serialization) and `create_notification_request_test.cpp`
   (validation) — `TEST_CASE`/`SECTION` naming, assertion style.
   **Comment cap (applies to every tier in this skill, Python included):** one line each,
   only where the reasoning isn't obvious from the code (e.g. why an assertion can't
   produce a false pass/fail from scheduling) — never a paragraph per test walking through
   what it does. If asked to *explain* a concept while adding a test, put the explanation
   in the reply, not in a comment block in the committed file.
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

## 4. Functional test procedure (handlers/, app.cpp)

The tier that covers `handlers/`/`app.cpp`: a **Python/pytest** suite in
`backend/tests/functional/` that talks to a real `atenciosamente_server` over a real
socket, backed by the real Postgres. It imports **nothing** from the project — only URLs,
methods and JSON — so it physically cannot cheat by calling a handler directly. Don't add
a `#include` or a CMake target to "improve" it; losing that property is the whole cost.

**Read these three before you run anything:**

1. ⚠️ **It is MANUAL. There is no CI job.** A regression here is caught only when someone
   runs the script. The wiring steps for adding the job later are in the `TODO(ci)` block
   at the top of `backend/scripts/functional.sh`; `PROJECT_PLAN.md` §8 marks the box as
   deliberately unbuilt and §10 carries the reasoning. Don't assume CI has your back, and
   don't silently build the CI job as a side effect of another task.
2. ⚠️ **It `TRUNCATE`s the dev `notifications` table before every test.** The autouse
   `clean_database` fixture does it, against the DB named by `POSTGRES_DB` — your dev
   database. Dev data does not survive a run.
3. ⚠️ **It must not run alongside a dev server or a `ctest` run.** The script refuses to
   start while `:8080` is occupied, but the `ctest` clash is uncaught — the `TRUNCATE`
   would yank rows out from under an integration test's open transaction.

**Procedure:**

1. File: `backend/tests/functional/test_<subject>.py`. Style reference:
   `test_notifications_api.py` (contract cases) and `test_concurrency.py` (parallel load).
2. Take the fixtures from `conftest.py`; don't build your own client. `api` is an
   `ApiClient` (its `post` takes a **raw `str`**, so bodies like `"not json"` are
   expressible); `make_api` is a factory for one client per thread, because
   `requests.Session` is not thread-safe; `clean_database` is autouse, so every test starts
   from an empty table and **no test may rely on rows another test wrote** — if it needs
   data, it POSTs it.
3. Assert error bodies via `res.json()["error"]`, never by comparing raw text — key order
   and whitespace are not part of the contract.
4. One test function per behaviour, so a failure names the part of the contract that broke.
   Use `@pytest.mark.parametrize` only for one behaviour with several inputs.
5. Nothing to wire: no CMake, no CTest, no `vcpkg.json`. The C++ build never learns this
   tier exists.
6. Run: `scripts/dev.sh functional` (format → configure → build → run), or
   `scripts/functional.sh` if already built. Subset with
   `scripts/functional.sh -- -k <substring>` — pass-through args work on `functional.sh`
   only, not through `dev.sh`. To run against a server you already have up (the one case
   `functional.sh` refuses), use `scripts/runManualFunctionalTests.sh` from
   `backend/scripts/`.
7. Both scripts are container-only; `functional.sh` cannot run from the WSL host (see its
   `TODO(host-run)`). Full manual walkthrough:
   `Documentation/phase-prompts/PHASE_3_FUNCTIONAL_END_TO_END_TESTS.md` §7.
8. Re-run it a few times before calling it done. This tier has real flake surface the C++
   tiers don't (startup races, port occupancy, read timeouts), so assert only on
   **outcomes** — counts, uniqueness, set equality — never on timing or interleaving.

## 5. Commit

One line, `Backend (Test): <what you covered>` (no body, no trailers) — e.g.
`Backend (Test): add integration test for insert() duplicate-title handling`. If the test
exposed a bug, fix it in a **separate** commit from the test that caught it, so the test's
own commit shows it passing against corrected behavior — matches this repo's "one focused
task per conversation" rhythm even within a single test-adding task.
