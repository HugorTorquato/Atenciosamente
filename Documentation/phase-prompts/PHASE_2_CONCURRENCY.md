# Phase 2 — Concurrency

> The plan for replacing connection-per-request with a thread-safe connection
> pool, so the DB layer is actually safe to share across Crow's already-running
> worker threads.
> This is a **plan document** — it describes the work and gives you a ready-to-paste
> prompt per step. It does not implement anything itself.
>
> Companion docs: attach [`PROJECT_PLAN.md`](../PROJECT_PLAN.md) to every step
> (it's the master decision log); the repo map is in
> [`reference/project_structure.md`](../reference/project_structure.md).

---

## 1. Goal & definition of done

**Goal:** the backend stops opening a brand-new `pqxx::connection` on every
request and instead checks one out of a small, fixed-size, **thread-safe** pool —
then returns it automatically when the request is done.

**Explicit non-goal:** async/coroutine handlers. Crow already runs
`.multithreaded()` (`main.cpp`) — a thread-pool-per-request model that's been
there since Phase 0. Phase 2 doesn't change that model; it only makes the DB
layer safe for those already-concurrent threads to share. (The roadmap's
"possibly async handlers" is the "possibly" we're declining for now — revisit
only if a future phase's traffic pattern actually needs it.)

Phase 2 is **done** when all of these are true:

- [ ] A `ConnectionPool` type owns a fixed number of open connections and is
      safe for multiple threads to call `acquire()` on concurrently.
- [ ] `acquire()` returns a move-only RAII lease; the connection returns to the
      pool automatically when the lease is destroyed — no manual "give it back"
      call anywhere.
- [ ] `acquire()` blocks (doesn't error or spin) when every connection is
      currently checked out, until one is returned.
- [ ] **Integration tests** prove the above under real concurrent `std::thread`s
      against a real pool of real connections.
- [ ] `GET`/`POST /notifications` go through the pool instead of
      `make_connection()` per request — both TODO comments in `connection.cpp`
      and `notifications.cpp` are resolved (deleted, not just left stale).
- [ ] Pool size is configurable via a `POSTGRES_POOL_SIZE` env var, with a
      documented default, wired into `.env.example` and CI.
- [ ] Every sub-task is committed, and new decisions are recorded in
      `PROJECT_PLAN.md` §10.

**What you'll learn:** `std::mutex`, `std::condition_variable`, the RAII-lease
pattern for pooled resources, and why a shared resource that was previously
"one per request" needs a lock the moment it becomes "shared across threads" —
the exact list from the roadmap (`PROJECT_PLAN.md` §6, Phase 2).

---

## 2. Where we are → where Phase 2 lands

Today, every request — `GET` or `POST /notifications` — calls `make_connection()`
(`backend/src/db/connection.cpp`) directly inside its handler
(`backend/src/handlers/notifications.cpp`), opens a fresh TCP socket, runs
Postgres's full auth handshake, uses it once, and lets it close when the
handler returns. That's correct (each request's connection is private to it —
no sharing, no race) but wasteful, and it's explicitly flagged as temporary:
both files carry a `// Phase 2 TODO` / `// TODO: Move away from one
connection-per-request` comment left by Phase 1.

Crow's server is **already** multithreaded (`app.port(8080).multithreaded().run()`
in `main.cpp`) — concurrent requests already land on different OS threads today.
There is no actual bug yet, because nothing is shared between those threads.
Phase 2 introduces the first thing that *is* shared: a pool of connections that
every thread draws from. The instant a resource is shared and mutated (checking
a connection out, marking it "in use", returning it) across threads, it needs a
lock — that's the whole reason `std::mutex` enters the project here and not
earlier.

**Good news — a lot is already wired:**

| Already in place | Where |
|---|---|
| `make_connection()` — builds one open `pqxx::connection` from `POSTGRES_*` env vars | `backend/src/db/connection.{hpp,cpp}` |
| The handshake-cost TODO, written in Phase 1, pointing at exactly this pool | `backend/src/db/connection.cpp` (top of `make_connection()`) |
| Crow's multithreaded server | `backend/src/main.cpp` |
| Handlers that already isolate their connection to one `pqxx::work` per request | `backend/src/handlers/notifications.cpp` |

So Phase 2 is **writing one new type and threading it through**, not
redesigning the request path.

---

## 3. Concepts you'll meet (this is new — read once)

You're comfortable with RAII and transactions from Phase 1. The new vocabulary:

- **Data race** — two threads touching the same memory at the same time, at
  least one of them writing, with no ordering between them. Undefined behavior
  in C++, not just "maybe wrong" — ASan/UBSan (already enabled in the `dev`
  preset) can catch some of these at runtime.
- **`std::mutex`** — a lock. `lock()` blocks until no other thread holds it;
  `unlock()` releases it. You'll almost never call these directly — RAII
  wrappers do it for you (see next point), the same spirit as never calling
  `pqxx::connection`'s close yourself.
- **`std::lock_guard` / `std::unique_lock`** — RAII for a mutex. `lock_guard` locks
  on construction, unlocks on destruction, full stop. `unique_lock` does the
  same but can additionally be handed to a `condition_variable` (next point)
  because it supports unlocking and relocking mid-scope.
- **`std::condition_variable`** — lets a thread **block** ("wait here") until
  another thread signals "something changed, go check again" — instead of
  spinning in a loop polling "is a connection free yet?". `acquire()` will
  `cv.wait(lock, predicate)` when the pool is empty; the lease's destructor will
  `cv.notify_one()` after returning its connection.
- **The RAII-lease pattern** — the same idea as `pqxx::work`'s auto-rollback,
  applied to a pooled resource instead of a transaction: a small move-only type
  (`ConnectionLease`) whose constructor removes one connection from the pool's
  available set and whose destructor puts it back. Callers never manage the
  checkout/checkin bookkeeping by hand, so there's no path where a bug leaks a
  connection out of the pool forever.
- **Why pool *connections*, not guard *one* connection with a mutex** — a single
  `pqxx::connection` can only run one query at a time; wrapping it in a mutex
  would serialize all DB access across every thread, which defeats the point of
  a multithreaded server. A pool of N connections lets up to N requests touch
  Postgres truly concurrently; the mutex only protects the bookkeeping of *which*
  connections are currently checked out, which is a fast, tiny critical section.

---

## 4. Step-by-step plan

Each step is one focused conversation that ends in a commit — the same rhythm
as Phases 0 and 1. Do them in order; later steps depend on earlier ones.

> **How to use the prompts:** finish and commit the current step first. Open a
> new conversation **in the WSL repo**. Attach `PROJECT_PLAN.md` and this file.
> Replace `[GIT LOG HERE]` with the output of `git log --oneline -10`. Paste the
> step's prompt.

---

### S1 — `ConnectionPool` type (the thread-safety core)

A new type that owns a fixed number of open connections, protected by a mutex,
with a blocking `acquire()` that hands out a move-only RAII lease.

- **Files:** `backend/src/db/connection_pool.{hpp,cpp}` (new) — e.g.
  `class ConnectionPool { public: explicit ConnectionPool(std::size_t size);
  ConnectionLease acquire(); ... };` and a nested/sibling `ConnectionLease` that
  exposes the underlying `pqxx::connection&` (via `operator*`/`operator->`) and
  returns it to the pool on destruction. Built out of N calls to the existing
  `make_connection()`. Register the new `.cpp` in `backend/CMakeLists.txt`
  (add it to `atenciosamente_core`, next to `connection.cpp`).
- **Skill:** none specific — fresh code; the `backend` subagent writes it,
  keeping the heavy explanatory-comment style of the existing `db/` code.
- **Decide & record:** the lease's exact API shape (does it expose a bare
  `pqxx::connection&`, or wrap `pqxx::work` creation itself?) and blocking vs.
  timeout/error behavior on exhaustion — recommend **blocking**, simplest thing
  that works at this traffic level; revisit only if it ever actually matters.

**▶ Prompt to implement this step**
```
Use the backend subagent to implement Step S1 of Phase 2.
Attached: PROJECT_PLAN.md and PHASE_2_CONCURRENCY.md. Recent history:
[GIT LOG HERE]

Goal: add a ConnectionPool type in a new db/connection_pool.{hpp,cpp} that owns
a fixed number of pqxx::connections (built via the existing make_connection()),
guarded by a std::mutex + std::condition_variable. acquire() returns a
move-only RAII ConnectionLease that returns its connection to the pool when
destroyed, and blocks (via the condition variable) when the pool is exhausted
instead of erroring or spinning. Wire the new .cpp into atenciosamente_core in
CMakeLists.txt. Explain the mutex/condition_variable/RAII-lease pattern as you
go — this is new to me.
When done: record the lease-shape and blocking-vs-timeout decisions in
PROJECT_PLAN.md §10 and commit in `Scope (Tag): summary` style (no body, no
trailers).
```

---

### S2 — Integration tests: prove the pool is actually thread-safe

Real concurrent threads, real connections, asserting the pool never hands the
same connection to two threads at once, and that `acquire()` blocks (not errors)
when exhausted until a release happens.

- **Files:** `backend/tests/integration/db/connection_pool_test.cpp` (new) — a
  new `db/` subfolder under `tests/integration/`, parallel to the existing
  `tests/integration/repository/`. Edit `backend/tests/CMakeLists.txt` to add
  the new source to the existing `tests_integration` executable (no new target
  needed — it already links `atenciosamente_core` + `libpqxx::pqxx` +
  `Catch2::Catch2WithMain`).
- **Skill:** none specific; the `backend` subagent writes it, following the
  existing `tests/integration/repository/notification_repository_test.cpp` style.
- **Decide & record:** this test needs a real DB (to construct real connections)
  even though it isn't testing SQL — log that the integration tier's rule is
  "needs a real DB," not strictly "tests the repository," so this still belongs
  in `tests_integration` and not a new tier.
- **Test shape:** construct a small pool (e.g. size 2), spawn more threads than
  that (e.g. 4) each doing `acquire()` → hold briefly → release, and assert no
  two threads ever observe the same connection as checked out simultaneously
  (e.g. via a small `std::atomic<int>` in-use counter per slot, or asserting
  `in_use <= pool_size` at all times from each thread). Separately, assert that
  acquiring from a fully-checked-out pool blocks until another thread releases
  (e.g. release on a short delay from one thread, measure that the waiting
  thread's `acquire()` didn't return before that delay).

**▶ Prompt to implement this step**
```
Use the backend subagent to implement Step S2 of Phase 2.
Attached: PROJECT_PLAN.md and PHASE_2_CONCURRENCY.md. Recent history:
[GIT LOG HERE]

Goal: add integration tests proving ConnectionPool (from S1) is thread-safe:
spawn more std::threads than the pool's size, each acquiring/holding/releasing
a lease, and assert the pool never over-commits (never more checked-out
connections than its size) and that acquire() blocks rather than errors when
exhausted. New file tests/integration/db/connection_pool_test.cpp, added as a
source to the existing tests_integration target in tests/CMakeLists.txt.
Follow the existing integration test style. Show me how you prove "never two
threads hold the same connection" without the test itself being flaky.
When done: record the integration-tier rationale in PROJECT_PLAN.md §10 and
commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### S3 — Wire the pool into the request path

Replace the per-handler `make_connection()` calls with `pool.acquire()`, and get
one pool instance from `main()` to the handlers without a global.

- **Files:**
  - `backend/src/main.cpp` — construct one `ConnectionPool` (size from config —
    see S4), own it on `main()`'s stack.
  - `backend/src/app.hpp`/`app.cpp` — `setup_routes(crow::SimpleApp&, ConnectionPool&)`;
    each `CROW_ROUTE(...)` registration becomes a lambda that captures `pool` by
    reference and forwards to the real handler, e.g.
    `CROW_ROUTE(app, "/notifications")([&pool](){ return handle_get_notifications(pool); });`
  - `backend/src/handlers/notifications.{hpp,cpp}` — `handle_get_notifications`
    and `handle_post_notification` gain a `ConnectionPool&` parameter; inside,
    `auto lease = pool.acquire(); pqxx::work txn{*lease};` replaces
    `pqxx::connection conn = make_connection(); pqxx::work txn{conn};`. Delete
    both `// TODO: Move away from one connection-per-request` comments.
- **Skill:** none specific; the `backend` subagent edits existing files.
- **Decide & record:** confirm the "pool lives on `main()`'s stack, threaded
  through by reference" shape (vs. a singleton/global) — this is the same "no
  globals" principle Phase 1 S2 already established for `make_connection()`.

**▶ Prompt to implement this step**
```
Use the backend subagent to implement Step S3 of Phase 2.
Attached: PROJECT_PLAN.md and PHASE_2_CONCURRENCY.md. Recent history:
[GIT LOG HERE]

Goal: wire the ConnectionPool (from S1) into the request path. Construct one
pool in main(), pass it by reference through setup_routes(app, pool) into
route-registration lambdas that capture it and call the handlers. Update
handle_get_notifications / handle_post_notification to take a ConnectionPool&
and call pool.acquire() instead of make_connection(). No global/singleton pool.
Delete the now-resolved "connection-per-request" TODO comments in
connection.cpp and notifications.cpp.
When done: update PROJECT_PLAN.md §10 if anything new was decided, and commit
in `Scope (Tag): summary` style (no body, no trailers).
```

---

### S4 — Pool size config

Make the pool's size a twelve-factor env var instead of a hardcoded number.

- **Files:** `.env.example` — add `POSTGRES_POOL_SIZE`; `main.cpp` — read it
  (reuse or adapt the existing `read_env`-style helper from `connection.cpp`,
  parsed to `std::size_t`); `.github/workflows/backend-ci.yml` — add
  `POSTGRES_POOL_SIZE` to the integration job's env so the new integration test
  (S2) and the server both see a value in CI.
- **Skill:** none specific.
- **Decide & record:** the default value, and the rationale for picking it
  (e.g. matching a small, deliberately modest number appropriate for a
  single-developer/CI workload today — not sized for production traffic that
  doesn't exist yet).

**▶ Prompt to implement this step**
```
Use the backend subagent to implement Step S4 of Phase 2.
Attached: PROJECT_PLAN.md and PHASE_2_CONCURRENCY.md. Recent history:
[GIT LOG HERE]

Goal: make the ConnectionPool's size configurable via a POSTGRES_POOL_SIZE env
var. Add it to .env.example with a sensible default, read it in main.cpp to
size the pool, and add it to the integration job's env in
.github/workflows/backend-ci.yml so CI's integration tests (S2) and server see
a value. Propose a default and rationale for me to confirm.
When done: record the default + rationale in PROJECT_PLAN.md §10 and commit in
`Scope (Tag): summary` style (no body, no trailers).
```

---

## 5. Decisions to make (record each in PROJECT_PLAN.md §10)

These aren't decided yet — resolve them during the step, then log them:

- **Lease API shape (S1)** — bare `pqxx::connection&` access vs. the lease
  itself building the `pqxx::work`. Recommended: expose the connection only;
  keep transaction creation at the call site, matching Phase 1's "transaction
  boundary explicit at the call site" repository decision.
- **Exhaustion behavior (S1)** — block (recommended) vs. timeout/error. Start
  simple; a timeout can be layered on later without changing the pool's shape.
- **Pool-size default (S4)** — a specific number, with rationale, not just
  "some number."
- **Integration-tier scope (S2)** — confirming a DB-touching-but-not-SQL test
  still belongs in `tests_integration`, not a new tier.

---

## 6. Files added / changed (map)

```
backend/
├── src/
│   ├── main.cpp                              S3  (construct pool, read S4 config)
│   ├── app.hpp / app.cpp                     S3  (setup_routes takes ConnectionPool&)
│   ├── db/
│   │   ├── connection.cpp                    S3  (delete resolved TODO)
│   │   └── connection_pool.{hpp,cpp}         S1  (new)
│   └── handlers/
│       └── notifications.{hpp,cpp}           S3  (handlers take ConnectionPool&; delete TODO)
├── CMakeLists.txt                            S1  (add connection_pool.cpp to core)
├── .env.example                              S4  (POSTGRES_POOL_SIZE)
└── tests/
    ├── CMakeLists.txt                        S2  (add new source to tests_integration)
    └── integration/
        └── db/
            └── connection_pool_test.cpp       S2  (new)

.github/workflows/backend-ci.yml              S4  (POSTGRES_POOL_SIZE in integration job env)
```

No mobile changes this phase — Phase 2 is backend-only per the roadmap.

---

## 7. How to execute

- **One focused conversation per step**, in order, each ending in a commit —
  the same rhythm as `PHASE_0_PROMPTS.md` / `PHASE_1_PERSISTENCE.md`.
- **Attach `PROJECT_PLAN.md`** (and this file) to every step, and back-port any
  new decision into its §10 decision log.
- **Delegate to the subagent:** the `backend` subagent owns S1–S4 — no frontend
  work this phase.
- Prefer to run each step **inside the WSL repo** so `/skill` commands and the
  subagent resolve.

When all steps are green and committed, tick the §1 checklist and Phase 2 is
done — next up is Phase 3 (scheduled notifications), per the roadmap.
