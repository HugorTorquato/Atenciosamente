# Atenciosamente — Project Plan & Architecture Decisions

> This document captures the planning conversation for the Atenciosamente project.
> It's a living record of *why* we made each choice, not just *what* was chosen.
> Update it whenever a decision changes.

---

## 1. Project overview

**Atenciosamente** is a mobile app (iOS + Android) that shows notifications to users.
The project is a **side project for learning**, not a race to ship — decisions favor
educational value alongside pragmatism.

**Scope, current:**
- Display notifications when the app opens (in-app feed)

**Scope, eventual:**
- Push notifications that arrive when the app is closed
- Whatever the brand grows into

---

## 2. Technology choices

| Layer | Choice | Why |
|---|---|---|
| **Mobile** | Flutter | One codebase for iOS + Android. Keeps the mobile layer *out of the way* so focus stays on the C++ backend. |
| **Backend language** | C++20 | Primary learning goal. Widely supported, has concepts/ranges/`<format>` without C++23 compiler gaps. |
| **HTTP framework** | Crow | Header-only-ish, Flask-like API, good middle ground between ease and realism. |
| **Database** | PostgreSQL | Already familiar from another project. Production-grade from day one. |
| **DB driver (C++)** | libpqxx | Modern C++ wrapper around libpq. |
| **JSON** | nlohmann/json | De-facto standard for modern C++. |
| **Test framework** | Catch2 v3 | Modern API, good CMake integration. |
| **Build system** | CMake + CMakePresets | Mandatory in modern C++. Presets keep commands simple. |
| **Dependency manager** | vcpkg | Manifest-based, huge catalog, integrates cleanly with CMake. |
| **Dev environment** | Docker + VS Code Dev Containers | Laptop stays clean; dev == prod from day one. |
| **Host OS (dev)** | Linux via WSL2 | C++ toolchain works best on Linux. WSL gives this on Windows. |
| **CI** | GitHub Actions | Familiar; we'll structure it to avoid past pain. |

---

## 3. Architecture (target state)

```
┌─────────────────┐         HTTP/JSON          ┌──────────────────┐
│   Mobile App    │ ◄────────────────────────► │   C++ Backend    │
│    (Flutter)    │                            │      (Crow)      │
└────────┬────────┘                            └────────┬─────────┘
         ▲                                              │
         │ push (later)                                 │ SQL
         │                                              ▼
┌────────┴────────┐                            ┌──────────────────┐
│   FCM / APNs    │ ◄──────────────────────────│   PostgreSQL     │
└─────────────────┘                            └──────────────────┘
```

Three backend responsibilities: **serve HTTP API**, **persist data**, **decide when
to send push notifications** (later phases).

---

## 4. Key architectural principles

These are the non-negotiables that cascade into every decision:

1. **Twelve-factor-ish.** Config from env vars. Logs to stdout. Stateless server.
   This is what lets us defer the deploy choice without painting ourselves into a corner.
2. **Dev == prod.** The container you build locally is (essentially) the container that
   runs in production. No "works on my machine" drift.
3. **Dumb data + free functions.** Structs hold data; behavior lives in free functions.
   Keeps types testable and evolvable. Opposite of Java-brained class design.
4. **Tests tiered by cost.** Unit tests for logic (fast, many). Integration tests for
   DB layer (slower, fewer). Functional tests for flows (slowest, fewest). Each tier
   compiled and run separately — this is the root cause fix for past CI flakiness.
5. **Walking skeleton first.** Every phase ships something working end-to-end. No
   phase is "just plumbing with nothing visible."

---

## 5. Repository layout (planned)

```
atenciosamente/
├── README.md
├── PROJECT_PLAN.md                 ← this file
├── .gitignore
├── .editorconfig
├── docker-compose.yml              ← dev stack: backend + postgres
├── .github/
│   └── workflows/
│       └── backend-ci.yml
│
├── backend/
│   ├── Dockerfile.dev              ← compiler, cmake, vcpkg, debuggers
│   ├── Dockerfile                  ← multi-stage prod build (stub, not yet used)
│   ├── CMakeLists.txt
│   ├── CMakePresets.json           ← `dev` and `ci` presets
│   ├── vcpkg.json
│   ├── .clang-format
│   ├── .clang-tidy
│   ├── migrations/
│   │   └── 0001_create_notifications.sql
│   ├── scripts/
│   │   ├── migrate.sh              ← idempotent migration runner
│   │   └── dev.sh
│   ├── src/
│   │   ├── main.cpp                ← thin entry point
│   │   ├── app.hpp / app.cpp       ← builds Crow app, registers routes
│   │   ├── domain/                 ← pure logic: no Crow, no libpqxx, no I/O
│   │   │   ├── notification.hpp
│   │   │   ├── notification_json.hpp / .cpp
│   │   │   └── create_notification_request.hpp / .cpp
│   │   ├── db/
│   │   │   └── connection.hpp / .cpp   ← make_connection(), RAII, connection-per-request
│   │   ├── repository/
│   │   │   └── notification_repository.hpp / .cpp   ← get_all() / insert(), free functions
│   │   └── handlers/
│   │       ├── notifications.hpp
│   │       └── notifications.cpp
│   └── tests/
│       ├── CMakeLists.txt
│       └── unit/
│           ├── notification_json_test.cpp
│           └── create_notification_request_test.cpp
│
└── mobile/
    └── atenciosamente_app/         ← output of `flutter create`
        ├── lib/
        │   ├── main.dart
        │   ├── api/
        │   │   └── notifications_client.dart
        │   └── screens/
        │       └── notifications_screen.dart
        └── pubspec.yaml
```

**Why monorepo:** solo project, API and client evolve together, one `git clone` bootstraps everything.

**Backend `src/` shape (added Phase 1):** `domain/` (pure data + validation +
serialization), `repository/` (SQL against an already-open `pqxx::work&`),
`db/` (opens the connection), `handlers/` (adapts one Crow endpoint to the
layers above). See `Documentation/reference/project_structure.md` for the
full annotated tree — this section is a map, not the source of truth for
every file.

---

## 6. Roadmap — phased, each phase ships something working

| Phase | Goal | Teaches |
|---|---|---|
| **0. Walking skeleton** | Crow server with one hardcoded endpoint; Flutter app fetches & displays. Docker Compose dev env. CI runs one unit test. | Modern C++ basics, CMake, vcpkg, Dev Containers |
| **1. Persistence** | Swap hardcoded data for Postgres. Add `POST /notifications`. Integration tests against real DB. | libpqxx, SQL migrations, RAII for DB resources, test isolation via transactions |
| **2. Concurrency** | Handle concurrent requests. Thread-safe DB layer. Possibly async handlers. | `std::mutex`, `std::shared_mutex`, connection pooling, thread safety |
| **3. Scheduled notifications** | Background worker decides "time to send this one." Still in-app delivery. | Background threads, design patterns, clock abstraction |
| **4. Push notifications** | FCM/APNs integration. Device tokens stored in DB. | Outbound HTTP, credential handling, production integration |
| **5+** | Auth, deployment, observability, profiling, whatever matters by then | TBD |

**Phase 0 explicit non-goals:** auth, rate limiting, HTTPS, real data, multiple endpoints,
state management on mobile, coverage gates, fancy logging. All come when they earn their place.

---

## 7. Deployment plan — the rungs of the ladder

We **deliberately deferred** choosing a deploy target. The twelve-factor-ish discipline
means we can climb this ladder one rung at a time when the project needs it:

1. **Local only** — `docker compose up` on laptop. Phone talks to laptop over LAN.
2. **Cheap VPS** — Hetzner / DigitalOcean / Linode, ~$5/mo. `git pull && docker compose up -d`.
3. **Reverse proxy** — Nginx or Caddy container for HTTPS + routing.
4. **Managed Postgres** — someone else handles backups, replication, PITR.
5. **Kubernetes / Cloud Run / etc.** — only if actually needed. A single small VPS handles
   a lot more traffic than most blog posts suggest.

Rule: we don't skip rungs prematurely.

---

## 8. Quality plan — testing + CI

### Three test tiers

```
    ┌─────────────────────────────┐   slow, realistic
    │  Functional / E2E tests     │      (Phase 2+)
    │  real HTTP + real Postgres  │
    ├─────────────────────────────┤
    │  Integration tests          │      (Phase 1+)
    │  repo layer + real DB       │
    ├─────────────────────────────┤
    │  Unit tests                 │      (Phase 0+)
    │  pure logic, mocks allowed  │   fast, isolated
    └─────────────────────────────┘
```

### CI structure — split jobs to contain flakiness

```
  on: push
     │
     ▼
┌─────────────┐
│    build    │   (cached: vcpkg deps, docker layers, cmake build dir)
└──────┬──────┘
       │
       ├──────────┬──────────────┬──────────────┐
       ▼          ▼              ▼              ▼
  ┌────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
  │  unit  │  │integration│  │functional│  │ lint /   │
  │ tests  │  │  tests   │   │  tests   │  │ format   │
  └────────┘  └──────────┘  └──────────┘  └──────────┘
              (Phase 1+)    (Phase 2+)
```

### Fixes for past CI pain

- **Postgres as a GitHub Actions `services:` container**, not manually installed.
- **Transaction-rollback test isolation** — `BEGIN` before each test, `ROLLBACK` after. No state leaks.
- **Functional tests use the same `docker-compose.yml`** as local dev. Eliminates environment drift.
- **Each tier in its own job** — one broken integration test doesn't hide a real unit test failure.
- **Cache aggressively** — vcpkg and Docker layers keyed on `vcpkg.json` and Dockerfile hashes.
- **Lint in parallel, not as a prerequisite.** `clang-format --dry-run --Werror` alongside tests.

### CI strictness policy

- **Hard gate:** tests that exist must pass.
- **No premature coverage thresholds** — chasing coverage on day one produces fake tests.
- **Pyramid grows with phases** — don't build the full pipeline before there's code to test.

---

## 9. Open questions / future decisions

Things we explicitly kicked down the road:

- **Deploy target** — decide when Phase 4–5 makes it real.
- **Auth** — not in scope until user accounts exist.
- **State management on Flutter** — no Riverpod/Bloc until we feel pain without it.
- **Observability** — logs to stdout is enough until it isn't.
- **Domain name** — when we deploy for real.
- **What Atenciosamente actually *is*** — the brand's identity and the content of notifications
  will shape everything downstream. The scaffolding here is deliberately content-agnostic.

---

## 10. Decision log

Running list of notable decisions, in order. New entries go at the bottom.

| Date | Decision | Rationale |
|---|---|---|
| 2026-04-21 | Project started as a learning-focused side project | — |
| 2026-04-21 | Flutter for mobile | Keep mobile layer simple; focus learning on C++ backend |
| 2026-04-21 | Crow + PostgreSQL from Phase 0 (not cpp-httplib/SQLite) | Existing Postgres familiarity makes the "teaching migration" unnecessary |
| 2026-04-21 | Monorepo (backend + mobile) | Solo project; API and client evolve together |
| 2026-04-21 | Dev inside Docker container from day one | Dev/prod parity; clean laptop |
| 2026-04-21 | VS Code + Dev Containers extension | Best-supported workflow for container dev |
| 2026-04-21 | C++20 | Modern enough for concepts/ranges/`<format>`, no C++23 compiler gaps |
| 2026-04-21 | Deployment target: deferred | Premature choice; twelve-factor discipline keeps options open |
| 2026-04-21 | CI: standard strictness, grows with phases | Avoid building full pipeline before there's code to test |
| 2026-04-23 | Project lives at `~/projects/Atenciosamente` in WSL2 native fs | Windows→Linux bind mounts cause severe I/O overhead for CMake + vcpkg |
| 2026-04-23 | `ubuntu:24.04` as Dockerfile.dev base | glibc required by vcpkg/Crow/libpqxx; ships gcc-13 + clang-18 via apt |
| 2026-04-23 | Non-root `dev` user at UID 1000 | Matches WSL2 uid; prevents root-owned files on bind-mounted workspace |
| 2026-04-23 | `userdel ubuntu` before creating `dev` | Ubuntu 24.04 image ships built-in `ubuntu` user at UID/GID 1000 — would collide |
| 2026-04-23 | `ca-certificates` in apt block | Required for git HTTPS inside a minimal Ubuntu container |
| 2026-04-23 | vcpkg cloned at `--depth 1` | Authoritative pin is `builtin-baseline` in vcpkg.json; full history not needed |
| 2026-04-23 | `depends_on: service_healthy` + `pg_isready` healthcheck | Naive `depends_on` only waits for container start, not Postgres readiness |
| 2026-04-23 | Named volume for Postgres data | Persists data across `docker compose down` without repo clutter |
| 2026-04-23 | clangd for IntelliSense + cpptools for debugging only | clangd is superior for C++ navigation; `C_Cpp.intelliSenseEngine` disabled to prevent conflict |
| 2026-04-25 | vcpkg manifest mode (`vcpkg.json`) | Portable, version-pinnable via `builtin-baseline`, integrates via CMake toolchain file |
| 2026-04-25 | `find_package(... CONFIG REQUIRED)` for all vcpkg packages | vcpkg installs CMake config files; MODULE mode would need hand-written Find modules |
| 2026-04-25 | Target-based CMake with `PRIVATE` compile flags | Modern CMake best practice; flags don't leak to consumers |
| 2026-04-25 | `-Wall -Wextra -Wpedantic -Werror` from day one | Zero-warning discipline is easier to maintain from the start than to clean up later |
| 2026-04-25 | CMakePresets `dev` (Debug + ASan/UBSan) and `ci` (Release) | Presets codify build config; dev and CI use identical commands |
| 2026-04-25 | ASan + UBSan in `dev` preset via `ENABLE_SANITIZERS` option | Catches memory and undefined-behavior bugs at near-zero perf cost during development |
| 2026-04-25 | `main.cpp` / `app.cpp` split | `main.cpp` is a thin entry point; `app.cpp` can link into a test target without pulling in `main()` |
| 2026-04-25 | `backend/contexts/` directory | Learning notes (cmake.md, docker.md) kept alongside code for quick reference |
| 2026-04-25 | Google style as `.clang-format` base, IndentWidth=4 | Well-defined baseline; 4-space indent overrides Google's 2-space |
| 2026-04-25 | `anthropic.claude-code` added to devcontainer extensions | Claude Code available inside the container without manual install after rebuild |
| 2026-04-25 | Phase 0 sub-task 2 complete: CMake + vcpkg + Crow hello world | `GET /` returns "hello"; builds and runs with `cmake --preset=dev && cmake --build --preset=dev` |
| 2026-04-26 | `created_at` as `std::chrono::system_clock::time_point` | Always UTC by the C++20 standard; avoids timezone ambiguity; serialized to ISO 8601 via `std::format` |
| 2026-04-26 | `to_json` / `serialize_notifications` as free functions in `notification_json.hpp/cpp` | Separates serialization from the data type; unit-testable without HTTP or a running server |
| 2026-04-26 | `std::span<const Notification>` for `serialize_notifications` parameter | Non-owning view; works with `vector`, `array`, or any contiguous range without coupling to ownership model |
| 2026-04-26 | `format_timestamp` and `sample_notifications` in anonymous namespaces | Internal linkage; not part of the public API; avoids accidental ODR violations across TUs |
| 2026-04-26 | Handler registered as a plain free function, not a lambda | Crow accepts any callable; named functions are easier to test and trace in stack dumps |
| 2026-04-26 | Explicit `Content-Type: application/json` header on responses | Crow defaults to `text/plain`; clients rely on this header to parse the body correctly |
| 2026-04-26 | Hardcoded sample data in Portuguese | Project is Brazilian; realistic content from day one |
| 2026-04-26 | Phase 0 sub-task 3 complete: `/notifications` endpoint with hardcoded JSON | `GET /notifications` returns a JSON array; serialization split from handler for testability |
| 2026-05-05 | Flutter SDK installed via tarball in WSL2, not snap | snap daemons unreliable in WSL2; tarball at `~/flutter/` is the recommended Linux path |
| 2026-05-05 | Flutter development on WSL2 host, not inside Docker | Flutter needs direct device/emulator access; containers can't bridge USB or wireless ADB |
| 2026-05-05 | Test device: Samsung Galaxy S20 FE, Android 13 | Real device preferred over emulator given hardware constraints; Android 13 enables wireless ADB |
| 2026-07-26 | Idempotent shell-script migration runner (`scripts/migrate.sh`), tracking applied migrations in `schema_migrations`, invoked by `dev.sh run`/`test` and (S6) CI | Trackable and repeatable unlike manual `psql <`; keeps schema migration out of the C++ binary unlike app-startup apply |
| 2026-05-05 | Wireless ADB for deployment to device | No USB passthrough complexity through WSL2; pairs once, reconnects over LAN |
| 2026-05-05 | Backend base URL for real device: `http://172.22.238.44:8080` | Phone on same LAN hits WSL2 host IP directly; Docker port binding forwards to Crow |
| 2026-05-05 | Android SDK installed via cmdline-tools tarball, not Android Studio | Android Studio not needed for real-device Flutter development; cmdline-tools + JDK 17 is sufficient |
| 2026-05-05 | Java 17 (openjdk-17-jdk) required by Android SDK tools | sdkmanager and Gradle are JVM programs; no JDK means no SDK toolchain |
| 2026-05-05 | `--platforms android` flag on `flutter create` | iOS/web/desktop dirs excluded; adding iOS later is one command |
| 2026-05-05 | `--org br.com.atenciosamente` reverse-domain app ID | Baked into AndroidManifest and Gradle; painful to change later |
| 2026-05-05 | `http` package (^1.2.2, resolved to 1.6.0) for HTTP client | Dart's standard HTTP package; minimal API, no code generation needed at Phase 0 |
| 2026-05-05 | JSON deserialization written by hand (`fromJson`) | Avoids code generation dependency at Phase 0; teaches the pattern before automating it |
| 2026-05-05 | `FutureBuilder` for async data fetching (no state management library) | Idiomatic Flutter for single async operations; Riverpod/Bloc deferred until we feel pain |
| 2026-05-05 | `StatelessWidget` for `NotificationsScreen` | FutureBuilder owns the async state internally; screen itself has no mutable state |
| 2026-05-05 | `_NotificationCard` extracted as a private widget | Keeps `build()` readable; Flutter convention favors small focused widgets |
| 2026-07-26 | Phase 1 S2: connection-per-request (`make_connection()` in `src/db/connection.{hpp,cpp}`), pooling deferred to Phase 2 | Opening latency is invisible at current traffic; pooling only earns its complexity once concurrent request volume makes it measurable — TODO left in `connection.cpp` marking where a pool would go |
| 2026-07-26 | `libpqxx::pqxx` linked `PUBLIC` on `atenciosamente_core` (not `PRIVATE` as on the server executable) | `atenciosamente_core`'s public header `db/connection.hpp` now `#include`s `<pqxx/connection>`; any consumer (server, `tests_unit`, and S5's `tests_integration`) needs libpqxx's include path and symbols transitively |
| 2026-07-26 | Phase 1 S3: repository shape is a free function `get_all(pqxx::work& txn)` in `src/repository/notification_repository.{hpp,cpp}`, not a class | Fits §4.3 "dumb data + free functions"; taking `pqxx::work&` (not a connection) makes the transaction boundary explicit at the call site instead of hidden in object state, and composes directly with S4's insert-in-same-transaction and S5's per-test rollback isolation — a class would tempt storing a `pqxx::connection` member, which conflicts with connection-per-request |
| 2026-07-26 | `notification_repository.cpp` selects `created_at` via `to_char(... AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS"Z"')` and parses the fixed string with `sscanf` + C++20 chrono calendar types, not `std::chrono::parse` | libstdc++ (GCC 13, this container) implements `<chrono>` formatting but not parsing; asking Postgres to format the timestamp in the query sidesteps both that gap and DateStyle-dependent output shapes |
| 2026-07-27 | Phase 1 S4: `insert(pqxx::work&, title, body)` wraps the `INSERT ... RETURNING id, created_at` in a `WITH inserted AS (...)` CTE, then re-`SELECT`s `id` and the same `to_char(...)` expression `get_all()` uses from `inserted` | A bare `RETURNING` can't apply `to_char(...)` formatting to the returned `created_at`; the CTE lets both the read and write paths format the timestamp identically, so `parse_created_at()` stays the single place that understands that string shape. `title`/`body` in the returned `Notification` are echoed back from the caller's arguments rather than re-selected, since the CTE's outer `SELECT` only needs the two DB-generated columns |
| 2026-07-27 | Used `txn.exec(sql, pqxx::params{txn, title, body})`, not `txn.exec_params(sql, title, body)` | The vcpkg-pinned libpqxx (8.x) marks `exec_params` `[[deprecated]]` in favor of `exec(query, params)`; `-Werror` turns the deprecation warning into a build failure. Same parameterized-query safety property either way — no user input is ever spliced into the SQL text |
| 2026-07-27 | POST /notifications validation extracted into a pure free function `parse_create_notification_request(const nlohmann::json&)` in new `src/domain/create_notification_request.{hpp,cpp}` (added to `atenciosamente_core`), and the handler parses the request body with `nlohmann::json::parse(req.body, nullptr, false)` instead of `crow::json::load` | Keeps validation logic unit-testable (`tests/unit/create_notification_request_test.cpp`) without a live server, Postgres, or linking Crow into `atenciosamente_core`; reuses the same JSON library (nlohmann) already used for response serialization instead of mixing two JSON libraries across the request/response boundary. `handlers/notifications.cpp` stays the thin adapter that turns a real `crow::request` into JSON and a `ValidationResult`/`Notification` into a `crow::response` |
| 2026-07-27 | Single-notification JSON serialization for the `201` response reuses the existing `to_json(const Notification&)` from `domain/notification_json.hpp` (already present since Phase 0, used internally by `serialize_notifications`) | No new serializer needed — `to_json` was already public and already the single-object building block; POST just calls it directly instead of only via the array wrapper |
| 2026-07-27 | New `backend/src/domain/` directory; moved all pure-logic files there together (`notification.hpp`, `notification_json.{hpp,cpp}`, `create_notification_request.{hpp,cpp}`) rather than leaving the new file alone at `src/` top level | Matches the existing `repository/` and `db/` naming — every `src/` subfolder now names a layer (`domain/` = pure data + validation + serialization, `repository/` = SQL, `db/` = connection, `handlers/` = HTTP adapters), so the four-layer shape is visible directly in the folder tree instead of only in prose. Moving all of them together (not just the newest file) keeps the layer boundary consistent instead of "some domain files are in `domain/`, some aren't" |
| 2026-07-28 | Phase 1 S5: integration tests (`backend/tests/integration/notification_repository_test.cpp`) run in a separate `tests_integration` CMake/CTest target from `tests_unit`, isolated per-`TEST_CASE` by opening their own `pqxx::work` and never calling `commit()` (relying on `pqxx::work`'s destructor to `ROLLBACK`), against the real Compose `db` service rather than a mock/fake DB | Keeps `tests_unit` runnable with no Postgres present (fast, CI-friendly before a DB is relevant) while `tests_integration` exercises the exact production SQL through `get_all()`/`insert()`; per-test rollback needs no teardown code and can't leak rows between tests or into the next `ctest` run, which was confirmed by checking `SELECT count(*) FROM notifications` was `0` both before and after two consecutive test runs |
| 2026-07-30 | Phase 1 S7: `AppNotification.toJson()` emits only `{title, body}`, not a mirror of every field | The backend's `POST /notifications` contract only accepts/validates `title`/`body` — `id`/`createdAt` are server-generated and don't exist before creation. A full mirror would require either nullable `id`/`createdAt` (weakening the model everywhere else it's used) or throwaway placeholder values just to populate a `toJson()`-able instance. `createNotification(String title, String body)` in `notifications_client.dart` takes plain args and builds the request JSON directly, matching `toJson()`'s shape without needing a dummy `AppNotification` instance |
| 2026-07-30 | `NotificationsScreen` promoted from `StatelessWidget` to `StatefulWidget` | Creating a notification needs the list to refetch after `CreateNotificationScreen` pops — a `StatelessWidget` calling `fetchNotifications()` inline inside `build()` has no hook to trigger that on demand. `State` holds the `Future` in a field and reassigns it via `setState()` after `Navigator.push(...)` resolves with `true`; still plain built-in `State`/`setState()`, not a state-management package |
| 2026-07-28 | Phase 1 S6: CI split into independent `unit` and `integration` jobs, no `needs:` between them | Each tier reports as its own status check (`PROJECT_PLAN.md` §8: "each tier in its own job"); vcpkg's binary cache makes a second full build cheap enough once warm that passing build artifacts between jobs (its own upload/download complexity, and a source of drift between two job filesystems) isn't worth it, and running in parallel gives the fastest total signal instead of delaying the integration signal behind however long `unit` takes |
| 2026-07-28 | `catch_discover_tests()` in `backend/tests/CMakeLists.txt` given `TEST_PREFIX "unit/"` / `TEST_PREFIX "integration/"` | Lets CI select one binary's tests with `ctest -R '^unit/'` / `-R '^integration/'` without touching the other's, keeping the existing `ctest --output-junit` + `upload-artifact` pattern intact rather than switching to direct binary invocation |
| 2026-07-28 | Integration CI job installs `postgresql-client` explicitly rather than assuming `psql` is preinstalled | `ubuntu-24.04` runner image ships a full PostgreSQL 16 server (disabled by default) but its documented "Installed apt packages" list doesn't name `postgresql-client` explicitly — checked, not assumed; cheap idempotent install either way |
| 2026-07-28 | Integration job's `services:` Postgres pinned to `postgres:16` with `POSTGRES_HOST=localhost` (not `db`), `POSTGRES_USER`/`PASSWORD`/`DB` matching `.env.example` verbatim | Matches the version already pinned in `docker-compose.yml` (not the `postgres:17` in the old `contexts/github-actions.md` sketch); GitHub Actions `services:` containers are reached via `localhost` on `ubuntu-24.04` runners (port published onto the runner's own network namespace), unlike the Compose network name `db` used for local dev |
