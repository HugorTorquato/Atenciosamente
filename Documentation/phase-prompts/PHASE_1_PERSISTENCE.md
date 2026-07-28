# Phase 1 — Persistence

> The plan for swapping hardcoded data for a real PostgreSQL database, plus a
> `POST /notifications` endpoint and the integration tests that prove it works.
> This is a **plan document** — it describes the work and gives you a ready-to-paste
> prompt per step. It does not implement anything itself.
>
> Companion docs: attach [`PROJECT_PLAN.md`](../PROJECT_PLAN.md) to every step
> (it's the master decision log); the repo map is in
> [`reference/project_structure.md`](../reference/project_structure.md).

---

## 1. Goal & definition of done

**Goal:** the backend stops returning hardcoded notifications and instead **reads from and
writes to PostgreSQL**. The mobile app gains a minimal "create notification" flow so the
whole loop is exercised end-to-end.

Phase 1 is **done** when all of these are true:

- [ ] A SQL migration creates the `notifications` table.
- [ ] `GET /notifications` returns rows **from Postgres** (no more `sample_notifications()`).
- [ ] `POST /notifications` inserts a new row and returns `201` with the created resource.
- [ ] A DB connection layer reads config from the `POSTGRES_*` env vars and manages the
      connection with **RAII** (no leaks, no globals).
- [ ] **Integration tests** run the repository against a real Postgres, isolated with
      per-test transaction rollback.
- [ ] **CI** spins up Postgres as a `services:` container and runs the integration tests as
      a **separate job** from the unit tests.
- [ ] The Flutter app can create a notification and see it appear in the feed.
- [ ] Every sub-task is committed, and new decisions are recorded in `PROJECT_PLAN.md` §10.

**What you'll learn:** libpqxx, SQL migrations, RAII for database resources, parameterized
statements, and transaction-based test isolation — the exact list from the roadmap
(`PROJECT_PLAN.md` §6, Phase 1).

---

## 2. Where we are → where Phase 1 lands

Today, `GET /notifications` is served entirely from memory. In
[`backend/src/handlers/notifications.cpp`](../../backend/src/handlers/notifications.cpp) a
function `sample_notifications()` returns three hardcoded `Notification` values, and the
handler serializes them. There is no database in the request path at all.

After Phase 1, the handler asks a **repository** for notifications, the repository runs a
SQL query through a **connection** built from environment config, and Postgres returns the
rows. A new `POST` handler does the reverse: it takes JSON, validates it, and inserts a row.

**Good news — a lot is already wired:**

| Already in place | Where |
|---|---|
| `libpqxx` dependency (the C++ Postgres driver) | `backend/vcpkg.json`, linked in `backend/CMakeLists.txt` |
| A running `postgres:16` database with a healthcheck | `docker-compose.yml` (`db` service) |
| DB credentials as env vars | `.env.example` — `POSTGRES_USER/PASSWORD/DB/HOST/PORT` |
| The `Notification` data type + JSON serialization | `backend/src/notification.hpp`, `notification_json.{hpp,cpp}` |

So Phase 1 is mostly **writing the layer between the handler and the database** — not
installing or configuring new infrastructure.

---

## 3. Concepts you'll meet (this is new — read once)

You're comfortable with backends but new to databases from C++. The vocabulary:

- **Connection string** — one text string that tells libpqxx how to reach Postgres:
  `"host=db port=5432 dbname=atenciosamente_dev user=... password=..."`. We build it from
  the `POSTGRES_*` env vars so nothing is hardcoded (twelve-factor, `PROJECT_PLAN.md` §4.1).
- **`pqxx::connection`** — an open connection to the database. Opening one is relatively
  expensive; for Phase 1 we open one **per request** and let it close when it goes out of
  scope. (Connection *pooling* is a Phase 2 concern — deliberately deferred.)
- **`pqxx::work` (a transaction)** — every query in libpqxx runs inside a transaction
  object. You call `txn.exec(...)`, then `txn.commit()`. If the `work` is destroyed without
  a commit, the transaction **rolls back** automatically. That auto-rollback is the whole
  trick behind our test isolation (§5).
- **RAII (Resource Acquisition Is Initialization)** — the C++ idea that a resource's
  lifetime is tied to an object's scope. `pqxx::connection` and `pqxx::work` are already
  RAII types: when they go out of scope, the connection closes / the transaction ends. Our
  job is to *use* that, not fight it — no manual open/close, no `new`/`delete`.
- **Parameterized (prepared) statements** — never build SQL by pasting strings together
  (`"... title='" + userInput + "'"` → SQL injection). Instead pass values as parameters:
  `txn.exec_params("INSERT INTO notifications(title, body) VALUES ($1, $2)", title, body)`.
  libpqxx escapes them safely.
- **Migration** — a SQL script that changes the database schema (e.g. "create the
  `notifications` table"). Migrations are ordered, forward-only, and committed to the repo
  so any clone / CI run can build the same schema.
- **Unit vs integration test** — a unit test checks pure logic with no database (fast, many).
  An integration test checks code that *talks to the database* against a real Postgres
  (slower, fewer). §5 goes deep on this — it's your explicit question.

---

## 4. Step-by-step plan

Each step is one focused conversation that ends in a commit — the same rhythm as Phase 0.
Do them in order; later steps depend on earlier ones. Every step lists the files it touches,
the skill to lean on, and a **▶ prompt you can paste** to start that step's conversation.

> **How to use the prompts:** finish and commit the current step first. Open a new
> conversation **in the WSL repo**. Attach `PROJECT_PLAN.md` and this file. Replace
> `[GIT LOG HERE]` with the output of `git log --oneline -10`. Paste the step's prompt.

---

### S1 — Database schema + migration

Create the `notifications` table via a checked-in SQL migration. Columns must match the
`Notification` struct and its JSON keys: `id`, `title`, `body`, `created_at`.

- **Files:** `backend/migrations/0001_create_notifications.sql` (new).
- **Skill:** `backend-add-migration`.
- **Decide & record:** the migration *mechanism* — how/when a migration is applied (a manual
  `psql <` step for now, a container init script, or app-startup application). Start simple;
  record the choice in `PROJECT_PLAN.md` §10.

```sql
-- 0001_create_notifications.sql  (shape — confirm id style during the step)
CREATE TABLE IF NOT EXISTS notifications (
    id          BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    title       TEXT        NOT NULL,
    body        TEXT        NOT NULL,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);
```

**▶ Prompt to implement this step**
```
Use the backend subagent to implement Step S1 of Phase 1.
Attached: PROJECT_PLAN.md and PHASE_1_PERSISTENCE.md. Recent history:
[GIT LOG HERE]

Goal: create the notifications table via a checked-in SQL migration whose columns
match the Notification struct (id, title, body, created_at).
Use the backend-add-migration skill. First propose a migration mechanism (how/when
migrations apply) and confirm it with me — it's an architectural decision.
When done: record the decision in PROJECT_PLAN.md §10 and commit in
`Scope (Tag): summary` style (no body, no trailers).
```

---

### S2 — Database connection layer (RAII)

A small module that builds the connection string from the `POSTGRES_*` env vars and hands
out a ready `pqxx::connection`. This is the seam every DB query goes through.

- **Files:** `backend/src/db/connection.{hpp,cpp}` (new) — a function like
  `pqxx::connection make_connection();` that reads env and returns an open connection.
  Register the new `.cpp` in `backend/CMakeLists.txt` (add it to `atenciosamente_core`).
- **Skill:** none specific — this is fresh code; the `backend` subagent writes it. Keep the
  heavy explanatory comment style of the existing code.
- **Decide & record:** connection-per-request now, **pooling deferred to Phase 2** — note it
  in §10 so the choice is explicit, not accidental.

**▶ Prompt to implement this step**
```
Use the backend subagent to implement Step S2 of Phase 1.
Attached: PROJECT_PLAN.md and PHASE_1_PERSISTENCE.md. Recent history:
[GIT LOG HERE]

Goal: add a db/ module that builds a libpqxx connection string from the POSTGRES_*
env vars and returns an open pqxx::connection, managed by RAII (no globals, no
manual close). Wire the new .cpp into atenciosamente_core in CMakeLists.txt.
Explain the RAII lifetime as you go — this is new to me. Use connection-per-request;
note that pooling is deferred to Phase 2.
When done: record decisions in PROJECT_PLAN.md §10 and commit in
`Scope (Tag): summary` style (no body, no trailers).
```

---

### S3 — Repository: the read path

Replace the hardcoded list. A repository function runs `SELECT ... FROM notifications`,
maps each row into a `Notification`, and returns them. Then the GET handler calls it.

- **Files:** `backend/src/repository/notification_repository.{hpp,cpp}` (new) — e.g.
  `std::vector<Notification> get_all(pqxx::work& txn);`. Edit
  `backend/src/handlers/notifications.cpp` to call the repository instead of
  `sample_notifications()`. Register the new `.cpp` in `CMakeLists.txt`.
- **Skill:** none specific (repository shape is a small design choice — free functions fit
  the project's "dumb data + free functions" principle, `PROJECT_PLAN.md` §4.3).
- **Decide & record:** repository shape (free functions vs a class) in §10.

**▶ Prompt to implement this step**
```
Use the backend subagent to implement Step S3 of Phase 1.
Attached: PROJECT_PLAN.md and PHASE_1_PERSISTENCE.md. Recent history:
[GIT LOG HERE]

Goal: add a notification repository that SELECTs notifications and maps rows to the
Notification struct, and change GET /notifications to use it instead of
sample_notifications(). Prefer free functions (project principle §4.3). Show me how
row-to-struct mapping works in libpqxx.
When done: record the repository-shape decision in PROJECT_PLAN.md §10 and commit in
`Scope (Tag): summary` style (no body, no trailers).
```

---

### S4 — `POST /notifications`: the write path (register new inputs)

The endpoint that "registers new inputs." It parses a JSON body, validates it, inserts a
row with a parameterized statement, and returns `201` with the created notification.

- **Files:** add `handle_post_notification(const crow::request&)` to
  `backend/src/handlers/notifications.{hpp,cpp}`; register the POST route in
  `backend/src/app.cpp`; add a repository `insert(...)` returning the new row
  (`INSERT ... RETURNING id, created_at`).
- **Skill:** `backend-add-endpoint`.
- **Details:** parse with `crow::json::load(req.body)`; return `400` on missing/invalid
  fields; use `exec_params` (never string concatenation); respond `201` + the JSON body.

**▶ Prompt to implement this step**
```
Use the backend subagent to implement Step S4 of Phase 1.
Attached: PROJECT_PLAN.md and PHASE_1_PERSISTENCE.md. Recent history:
[GIT LOG HERE]

Goal: add POST /notifications — parse+validate the JSON body, INSERT with a
parameterized statement (RETURNING id, created_at), respond 201 with the created
resource, and 400 on invalid input. Reuse the repository from S3.
Use the backend-add-endpoint skill. Explain parameterized statements / SQL-injection
safety as you go.
When done: update PROJECT_PLAN.md §10 if anything was decided, and commit in
`Scope (Tag): summary` style (no body, no trailers).
```

---

### S5 — Integration tests

Prove the repository works against a real database. A new **separate** test target runs
each test inside a transaction that is **rolled back** at the end, so tests never leak state
into each other or leave rows behind.

- **Files:** `backend/tests/integration/notification_repository_test.cpp` (new); edit
  `backend/tests/CMakeLists.txt` to add a `tests_integration` executable (parallel to the
  existing `tests_unit`), linking `atenciosamente_core` + `libpqxx::pqxx` +
  `Catch2::Catch2WithMain`, and `catch_discover_tests(tests_integration)`.
- **Skill:** none specific; the `backend` subagent writes it, following the existing
  `tests/unit/notification_json_test.cpp` Catch2 style.
- **Isolation pattern:** open a connection, `pqxx::work txn`, do inserts/queries through
  `txn`, assert, and **never commit** — when `txn` is destroyed the changes roll back.
- **Run locally:** `docker compose up -d db`, then run `tests_integration` with the
  `POSTGRES_*` env pointing at it.

**▶ Prompt to implement this step**
```
Use the backend subagent to implement Step S5 of Phase 1.
Attached: PROJECT_PLAN.md and PHASE_1_PERSISTENCE.md. Recent history:
[GIT LOG HERE]

Goal: add integration tests for the repository against a real Postgres, isolated by
per-test transaction rollback (never commit). Create a separate tests_integration
target in tests/CMakeLists.txt (parallel to tests_unit). Follow the existing Catch2
style. Show me how the rollback isolation works.
When done: update PROJECT_PLAN.md §10 if needed and commit in
`Scope (Tag): summary` style (no body, no trailers).
```

---

### S6 — CI wiring

Teach CI to run the integration tests. Add Postgres as a GitHub Actions `services:`
container and run integration tests as their **own job**, separate from unit tests, so one
flaky DB test can't mask a real unit failure (`PROJECT_PLAN.md` §8).

- **Files:** `.github/workflows/backend-ci.yml` — add a `services: postgres:16` block with a
  health check, set the `POSTGRES_*` env for the test step, and split into `unit` and
  `integration` jobs (both depend on the build). Keep the existing vcpkg binary-cache setup.
- **Skill:** none specific; the `backend` subagent edits the workflow.

```yaml
# sketch — the integration job gains a service container:
services:
  db:
    image: postgres:16
    env:
      POSTGRES_USER: atenciosamente
      POSTGRES_PASSWORD: devpassword
      POSTGRES_DB: atenciosamente_dev
    ports: ["5432:5432"]
    options: >-
      --health-cmd "pg_isready -U atenciosamente -d atenciosamente_dev"
      --health-interval 5s --health-timeout 5s --health-retries 5
```

**▶ Prompt to implement this step**
```
Use the backend subagent to implement Step S6 of Phase 1.
Attached: PROJECT_PLAN.md and PHASE_1_PERSISTENCE.md. Recent history:
[GIT LOG HERE]

Goal: update .github/workflows/backend-ci.yml to run integration tests. Add a
postgres:16 services container with a health check, wire the POSTGRES_* env, apply
the S1 migration before the tests, and run integration tests as a SEPARATE job from
unit tests (per PROJECT_PLAN §8). Keep the existing vcpkg binary caching.
When done: commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### S7 — Mobile: create-notification flow

Close the loop: let the app POST a new notification and see it in the feed. Kept
deliberately minimal — the app exists to exercise the backend.

- **Files:** add a POST function to `mobile/atenciosamente_app/lib/api/notifications_client.dart`;
  add `toJson()` to `lib/models/notification.dart`; add a simple form screen under
  `lib/screens/` and reach it via `Navigator` from the list screen.
- **Skills:** `frontend-add-model` (for `toJson`), `frontend-add-screen` (for the form).

**▶ Prompt to implement this step**
```
Use the frontend subagent to implement Step S7 of Phase 1.
Attached: PROJECT_PLAN.md and PHASE_1_PERSISTENCE.md. Recent history:
[GIT LOG HERE]

Goal: add a minimal "create notification" flow — a POST helper in the api client, a
toJson() on the model, and a small form screen reached via Navigator from the list.
After creating, return to the feed and show the new item. Keep it minimal, no state
management library. Use the frontend-add-model and frontend-add-screen skills.
When done: commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

## 5. Testing: unit vs integration (your explicit question)

**Short answer:** you need **both**, and Phase 1 is where **integration** tests enter the
project. They test different things and run in different jobs.

| | **Unit test** | **Integration test** |
|---|---|---|
| What it checks | Pure logic — no I/O | Code that talks to Postgres (the repository) |
| Database? | No | **Yes — a real Postgres** |
| Speed / count | Fast, many | Slower, fewer |
| Example here | `serialize_notifications()` produces the right JSON (`tests/unit/notification_json_test.cpp`) | `get_all()` / `insert()` actually read/write rows (`tests/integration/...`) |
| Runs in | The existing `tests_unit` target / `unit` CI job | The new `tests_integration` target / `integration` CI job |
| Isolation | None needed (no shared state) | **Per-test transaction rollback** |

**Why the repository needs integration tests, not unit tests:** the whole point of the
repository is the SQL — the column names, the row→struct mapping, the `INSERT ... RETURNING`.
A "unit test" with a fake database would only test your fake, not the real query. The bugs
here (a typo'd column, a NULL you didn't expect) only surface against real Postgres. So the
repository is tested by **hitting a real database**.

**The isolation trick (why tests don't pollute each other):** each test opens a
`pqxx::work` transaction, does its inserts/queries, asserts — and **never commits**. When
the transaction object is destroyed, Postgres rolls everything back. The next test starts
from a clean slate. No `DELETE FROM` cleanup, no order dependence, no leftover rows. This is
exactly the "`BEGIN` before each test, `ROLLBACK` after" rule from `PROJECT_PLAN.md` §8.

**Keep pure logic as unit tests.** JSON serialization, validation helpers, string
formatting — anything with no database — stays in `tests_unit`. Don't make it an integration
test just because it's nearby; that would make the fast suite slow for no benefit.

---

## 6. CI configuration detail

The current workflow (`.github/workflows/backend-ci.yml`) has a single `build-and-test` job:
install tools → install vcpkg → `cmake --preset ci` → build → `ctest --preset ci`. Phase 1
extends it along the plan in `PROJECT_PLAN.md` §8:

1. **Add Postgres as a `services:` container** (see the sketch in S6) — GitHub Actions
   starts it alongside the job and waits for the health check. No manual install.
2. **Split unit and integration into separate jobs.** Both depend on the build. The `unit`
   job runs `tests_unit`; the `integration` job runs `tests_integration` with the
   `POSTGRES_*` env pointing at the service container, after applying the S1 migration. A
   broken integration test then fails *its* job without hiding a unit-test signal.
3. **Keep the caching.** The vcpkg binary cache (`VCPKG_BINARY_SOURCES`) and build already
   configured stay as-is — the DB service is additive.
4. **Run the migration before integration tests** so the schema exists (the mechanism chosen
   in S1 decides exactly how — e.g. `psql < backend/migrations/0001_*.sql`).

You don't have to build the full three-tier pyramid now — functional/E2E tests are Phase 2+.
Phase 1 adds exactly the **integration** rung.

---

## 7. Files added / changed (map)

```
backend/
├── migrations/
│   └── 0001_create_notifications.sql        S1  (new)
├── src/
│   ├── domain/
│   │   └── create_notification_request.{hpp,cpp}  S4  (new; validation, no Crow/DB)
│   ├── db/
│   │   └── connection.{hpp,cpp}             S2  (new)
│   ├── repository/
│   │   └── notification_repository.{hpp,cpp} S3/S4 (new)
│   └── handlers/
│       └── notifications.{hpp,cpp}          S3 edit + S4 POST handler
│   └── app.cpp                              S4  (register POST route)
├── CMakeLists.txt                           S2/S3/S4 (add new .cpp to core)
└── tests/
    ├── CMakeLists.txt                       S4  (add validation unit test) / S5 (add tests_integration target)
    ├── unit/
    │   └── create_notification_request_test.cpp  S4  (new)
    └── integration/
        └── notification_repository_test.cpp S5  (new)

.github/workflows/backend-ci.yml             S6  (Postgres service + job split)

mobile/atenciosamente_app/lib/
├── api/notifications_client.dart            S7  (POST helper)
├── models/notification.dart                 S7  (toJson)
└── screens/…                                S7  (create-notification form)
```

---

## 8. Decisions to make (record each in PROJECT_PLAN.md §10)

These aren't decided yet — resolve them during the step, then log them:

- **Migration mechanism (S1)** — manual `psql`, container init script, or app-startup apply?
  Start simple; upgrade later.
- **Connection-per-request vs pool (S2)** — per-request for Phase 1; **pooling deferred to
  Phase 2**. Log it so it's a choice, not an oversight.
- **Repository shape (S3)** — free functions (fits §4.3 "dumb data + free functions") vs a
  class. Recommended: free functions taking a `pqxx::work&`.
- **Test-database provisioning (S5/S6)** — reuse the compose `db` locally and the `services:`
  container in CI; how the schema gets created before tests run.

---

## 9. How to execute

- **One focused conversation per step**, in order, each ending in a commit — the same rhythm
  as `PHASE_0_PROMPTS.md`.
- **Attach `PROJECT_PLAN.md`** (and this file) to every step, and back-port any new decision
  into its §10 decision log.
- **Delegate to the subagents:** the `backend` subagent owns S1–S6, the `frontend` subagent
  owns S7. They know when to reach for the task skills.
- **Skills that do the heavy lifting:** `backend-add-migration` (S1), `backend-add-endpoint`
  (S4), `frontend-add-model` + `frontend-add-screen` (S7). The other steps are fresh code the
  subagent writes directly, following the existing file styles.
- Prefer to run each step **inside the WSL repo** so `/skill` commands and the subagents
  resolve.

When all steps are green and committed, tick the §1 checklist and Phase 1 is done — next up
is Phase 2 (concurrency), per the roadmap.
