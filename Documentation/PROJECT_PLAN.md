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
│   ├── Dockerfile                  ← multi-stage prod build (stub in Phase 0)
│   ├── CMakeLists.txt
│   ├── CMakePresets.json           ← `dev` and `ci` presets
│   ├── vcpkg.json
│   ├── .clang-format
│   ├── .clang-tidy
│   ├── src/
│   │   ├── main.cpp                ← thin entry point
│   │   ├── app.hpp / app.cpp       ← builds Crow app, registers routes
│   │   └── handlers/
│   │       ├── notifications.hpp
│   │       └── notifications.cpp
│   ├── include/                    ← public headers (empty in Phase 0)
│   └── tests/
│       ├── CMakeLists.txt
│       └── unit/
│           └── notifications_test.cpp
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
