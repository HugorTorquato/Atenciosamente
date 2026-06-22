# Phase 0 — Sub-task prompts

> This file contains the prompts for each Phase 0 sub-task. Each one starts a
> **new conversation** with Claude, focused on one step of the walking skeleton.
> The master conversation captured the architecture; these prompts execute it.

---

## How to use this file

1. Finish whatever sub-task you're on **and commit to git** before starting the next one.
2. Open a new Claude conversation.
3. **Attach `PROJECT_PLAN.md`** to the conversation (drag-drop or the attachment button).
   Don't paste it into the prompt body — attachments are handled better.
4. **Paste the numbered prompt below** into the first message.
5. **Replace the `[GIT LOG HERE]` placeholder** with the actual output of:
   ```bash
   cd ~/projetos/atenciosamente && git log --oneline -10
   ```
6. Work through the conversation. Ask questions. Push back.
7. At the end, update the decision log in `PROJECT_PLAN.md` with any new choices
   made during the conversation.
8. Commit, and move to the next prompt.

### The "escape hatch"

If a sub-task conversation exposes a problem with a **prior architectural
decision** — e.g., "wait, vcpkg in Docker is painful, should we reconsider?" —
stop, don't force a fix in the sub-task. Come back to the master conversation,
update `PROJECT_PLAN.md`, then re-run the sub-task with updated context.

### When to deviate

These prompts are a plan, not a contract. If a sub-task feels too big, split it.
If two feel small, combine them. The goal is "one focused conversation per
meaningful chunk of work" — not six specifically.

---

## Prompt 1 — Docker dev environment

**Goal:** A working `docker compose up --build` that brings up a Postgres container
and a (still empty) backend dev container with the full C++20 toolchain. VS Code
Dev Containers opens the project inside that container.

```
I'm working on a learning-focused C++ side project called **Atenciosamente**. Full
context is in the attached PROJECT_PLAN.md — please read it first; it contains
the tech stack, architecture, roadmap, and decision log.

## Your role

Act as a senior C++ / DevOps engineer mentoring me. This is a learning project.
I want to understand *every* decision, not just get working files. Explain file
by file, with rationale, before moving on. Ask questions when choices are
non-obvious. Push back if I suggest something wrong.

## Current state

The repo lives at `~/projetos/atenciosamente/` inside WSL2 (Ubuntu). Recent git:

[GIT LOG HERE]

Folder skeleton already created (empty):

```
backend/{src/handlers,include,tests/unit}
mobile/
.github/workflows/
```

## Goal of THIS conversation

Set up the **Docker-based development environment** for the C++ backend, per
Phase 0 of the roadmap. By the end of this conversation I should be able to:

1. Run `docker compose up --build` from the repo root.
2. Get a running Postgres container and a running (empty-for-now) backend
   container.
3. Open the project in VS Code with the Dev Containers extension and have it
   drop me into a shell inside the backend container, with C++20 toolchain,
   CMake, Ninja, vcpkg, clang-format, clang-tidy, and gdb all installed.
4. Understand what every line of every file does.

## Files we'll create

- `backend/Dockerfile.dev` (C++ toolchain image for development)
- `docker-compose.yml` (dev stack: backend + postgres)
- `.devcontainer/devcontainer.json` (VS Code integration)
- `.env.example` (committed; real `.env` stays local and gitignored)

## Out of scope for this conversation

- Multi-stage production Dockerfile (Phase 0 stub only — we'll do production
  later, when we're ready to deploy)
- CMakeLists.txt, vcpkg.json, or any C++ code (next conversation)
- CI workflow (a later conversation)
- Flutter setup (separate conversation)

## Process

1. Walk me through each file before writing it.
2. Explain non-obvious choices (base image, user permissions, vcpkg install
   strategy, volume mounts, network setup, healthchecks, etc.).
3. Pause for my input at decision points.
4. At the end, give me the exact commands to build, start, and verify the
   environment.
5. Give me a diff/update for PROJECT_PLAN.md's decision log to capture any
   non-trivial choices we made.

Ready when you are. Start by asking me any questions you need to make good
decisions about the Dockerfile (base distro, user strategy, whether to pin
vcpkg to a specific commit, etc.).
```

---

## Prompt 2 — CMake + vcpkg + "hello world" Crow server

**Goal:** Add the C++ build system and a minimal Crow server that responds to
`GET /` with a plain-text "hello". Builds and runs inside the dev container.

```
I'm working on a learning-focused C++ side project called **Atenciosamente**. Full
context is in the attached PROJECT_PLAN.md — please read it first.

## Your role

Senior C++ engineer mentoring me. Learning project. Explain every decision,
especially modern CMake and vcpkg idioms. Ask questions when non-obvious.
Push back if I suggest something wrong.

## Current state

Docker dev environment is set up and working — I can `docker compose up` and
get a shell inside the backend container. Recent git:

[GIT LOG HERE]

The backend container has: g++ 12+, CMake 3.25+, Ninja, vcpkg, clang-format,
clang-tidy, gdb. No C++ code exists yet.

## Goal of THIS conversation

Stand up the C++ build system and a minimal working Crow server. By the end:

1. `vcpkg.json` declares Crow, nlohmann/json, libpqxx, and Catch2.
2. Top-level `backend/CMakeLists.txt` configures C++20, warnings-as-errors,
   finds dependencies, declares the `atenciosamente_server` executable.
3. `backend/CMakePresets.json` has `dev` (Debug + sanitizers) and `ci` (Release)
   presets.
4. `backend/src/main.cpp` + `backend/src/app.hpp/cpp` — minimal Crow app that
   returns "hello" on GET /.
5. Running `cmake --preset=dev && cmake --build --preset=dev` builds cleanly.
6. Running the binary, then `curl http://localhost:8080/` returns "hello".
7. `backend/.clang-format` and `backend/.clang-tidy` configured.

## Files we'll create

- `backend/vcpkg.json`
- `backend/CMakeLists.txt`
- `backend/CMakePresets.json`
- `backend/.clang-format`
- `backend/.clang-tidy`
- `backend/src/main.cpp`
- `backend/src/app.hpp`
- `backend/src/app.cpp`

## Out of scope

- The `/notifications` endpoint and JSON serialization (next conversation)
- Tests (conversation after that)
- Concurrency tuning
- Production Dockerfile

## Process

1. Walk me through each file before writing it.
2. Explain modern CMake choices: target-based vs global, `find_package` with
   CONFIG vs MODULE, why `CMakePresets` over shell scripts, etc.
3. Explain vcpkg manifest mode vs classic mode.
4. Explain the split between `main.cpp` and `app.cpp` (testability later).
5. Pause at decision points.
6. Give me the exact build + run commands at the end.
7. Update the decision log in PROJECT_PLAN.md.

Start by confirming the Docker environment is working, then asking any
questions you need about specific choices (e.g., CMake target structure,
whether Crow is already available via vcpkg or needs a pin).
```

---

## Prompt 3 — `/notifications` endpoint with hardcoded JSON

**Goal:** A real endpoint returning a JSON array of notifications, using the
"dumb data + free functions" principle.

```
I'm working on a learning-focused C++ side project called **Atenciosamente**. Full
context is in the attached PROJECT_PLAN.md — please read it first.

## Your role

Senior C++ engineer mentoring me. Learning project. This conversation is
mostly about modern C++ idioms (value types, free functions, JSON) rather
than infrastructure. Explain every choice.

## Current state

Docker dev environment + Crow "hello world" are working. Recent git:

[GIT LOG HERE]

`backend/src/main.cpp` and `backend/src/app.cpp` exist with a minimal Crow
setup. No handlers yet beyond `GET /`.

## Goal of THIS conversation

Implement the Phase 0 `/notifications` endpoint. By the end:

1. A `Notification` struct in `backend/src/notification.hpp` with fields
   `id`, `title`, `body`, `created_at`.
2. Free functions `to_json(const Notification&)` and
   `serialize_notifications(std::span<const Notification>)` in a separate
   `notification_json.hpp/cpp` (so we can unit test serialization without
   HTTP).
3. A handler function in `backend/src/handlers/notifications.hpp/cpp` that
   returns a hardcoded `std::vector<Notification>` serialized to JSON.
4. `app.cpp` registers the handler at `GET /notifications`.
5. `curl http://localhost:8080/notifications` returns a JSON array with 3+
   sample notifications.
6. Responses have correct `Content-Type: application/json`.

## Files we'll create / edit

- `backend/src/notification.hpp` (new)
- `backend/src/notification_json.hpp` (new)
- `backend/src/notification_json.cpp` (new)
- `backend/src/handlers/notifications.hpp` (new)
- `backend/src/handlers/notifications.cpp` (new)
- `backend/src/app.cpp` (edit — register handler)
- `backend/CMakeLists.txt` (edit — add new sources)

## Out of scope

- Database (Phase 1)
- Tests (next conversation — but we'll write the code in a testable way)
- POST endpoint (Phase 1)
- Pagination, filtering, auth (later)

## Process

1. Start by discussing the `Notification` struct — what fields, what types
   (especially for `created_at` — `std::chrono` vs string).
2. Discuss the "dumb data + free functions" split and why.
3. Walk me through nlohmann/json's ADL-based serialization.
4. Show me the handler, explain Crow's request/response model.
5. Pause at decision points.
6. Verify end-to-end with curl at the end.
7. Update the decision log in PROJECT_PLAN.md.

Start by asking about the `Notification` struct design — especially how to
represent `created_at`.
```

---

## Prompt 4 — First Catch2 unit test + tests/CMakeLists.txt

**Goal:** A seed unit test that proves the test-build-run loop works. Tests the
JSON serialization function (pure logic, no HTTP, no DB).

```
I'm working on a learning-focused C++ side project called **Atenciosamente**. Full
context is in the attached PROJECT_PLAN.md — please read it first.

## Your role

Senior C++ engineer mentoring me. Learning project. This conversation is
about setting up the testing foundation correctly — it will pay off across
every later phase.

## Current state

The Crow server is working, `/notifications` returns hardcoded JSON. Recent git:

[GIT LOG HERE]

Catch2 is already declared in `vcpkg.json` and available. No tests exist yet.

## Goal of THIS conversation

Stand up the unit test infrastructure with one real test. By the end:

1. `backend/tests/CMakeLists.txt` declares a `tests_unit` executable.
2. `backend/tests/unit/notification_json_test.cpp` uses Catch2 to test
   `serialize_notifications` with at least: empty vector, single notification,
   multiple notifications, verification of JSON structure.
3. `ctest --preset=dev` runs the tests and passes.
4. Tests are registered with CTest via `catch_discover_tests`.
5. Top-level `backend/CMakeLists.txt` conditionally adds the `tests/`
   subdirectory (so production builds don't include them).
6. I understand the three-tier test strategy and why this test belongs in
   the "unit" tier.

## Files we'll create / edit

- `backend/tests/CMakeLists.txt` (new)
- `backend/tests/unit/notification_json_test.cpp` (new)
- `backend/CMakeLists.txt` (edit — add tests subdirectory guarded by option)
- `backend/CMakePresets.json` (edit — ensure `dev` preset builds tests)

## Out of scope

- Integration tests against Postgres (Phase 1)
- Functional tests against running server (Phase 2)
- Code coverage reporting (later)
- Mocking framework (later — and probably not needed at all)

## Process

1. Walk me through Catch2 v3 style: `TEST_CASE`, `SECTION`, `REQUIRE`.
2. Explain why production source and test executables need to share code
   (probably via an OBJECT library or a proper library target), and which
   pattern fits our size.
3. Explain `catch_discover_tests` vs `add_test` — why we use it.
4. Explain the `BUILD_TESTING` convention and the `option()` command.
5. Show me the CTest output and how to run a subset (filters, tags).
6. Pause at decision points.
7. Update the decision log in PROJECT_PLAN.md.

Start by asking how I want to structure the production code as a library that
tests can link against (OBJECT library vs STATIC library vs INTERFACE), and
what naming convention I want for test files.
```

---

## Prompt 5 — GitHub Actions CI

**Goal:** A minimal CI pipeline that builds the backend and runs unit tests in
a GitHub Actions runner, using the same Dockerfile as local dev.

```
I'm working on a learning-focused C++ side project called **Atenciosamente**. Full
context is in the attached PROJECT_PLAN.md — please read it first. Pay special
attention to Section 8 (quality plan + CI structure).

## Your role

Senior DevOps engineer mentoring me. I have past pain with GitHub Actions and
functional tests — see PROJECT_PLAN.md Section 8 for the specific fixes we're
committing to. Explain every choice.

## Current state

Backend builds, one unit test passes locally. Recent git:

[GIT LOG HERE]

Repo is on GitHub at [I'll fill in the URL]. No workflows exist yet.

## Goal of THIS conversation

Set up a minimal, correct, well-structured CI pipeline. By the end:

1. `.github/workflows/backend-ci.yml` runs on push + PR to `main`.
2. One job for now: `build-and-test`, running in a container built from
   `backend/Dockerfile.dev` (or a close equivalent to keep CI == local dev).
3. Steps: checkout → cache vcpkg deps → configure with `ci` preset → build →
   run unit tests → upload test results.
4. Docker layer caching or vcpkg caching is working (builds are fast after
   the first run).
5. Structured so Phase 1's integration-test job, Phase 2's functional-test
   job, and a separate lint/format job can be *added* without restructuring.
6. Red tests block merges (branch protection rule — I'll set that up in the
   GitHub UI; you tell me which settings to flip).

## Files we'll create / edit

- `.github/workflows/backend-ci.yml` (new)
- Possibly `.github/dependabot.yml` (optional, for vcpkg baseline updates)

## Out of scope

- Postgres integration-test job (Phase 1 adds this)
- Functional-test job against docker compose (Phase 2)
- Coverage reporting (maybe later)
- Deployment/publish jobs (much later)
- Flutter CI (separate workflow when we set up mobile)

## Process

1. Walk me through the workflow YAML top to bottom.
2. Explain caching strategies (which to use for vcpkg — the official vcpkg
   binary cache on GitHub Actions cache? or docker layer cache? tradeoffs?).
3. Explain why we run in a container vs on the runner directly.
4. Explain concurrency controls (cancel superseded runs on the same branch).
5. Explain the `permissions:` block and least-privilege.
6. Show me the exact branch protection settings to enable.
7. Update the decision log in PROJECT_PLAN.md.

Start by asking how I want to run the build — inside a container built from
Dockerfile.dev, or on the runner with a vcpkg install action — and which
caching strategy fits best.
```

---

## Prompt 6 — Flutter app scaffold + fetch-and-display

**Goal:** A Flutter app that calls `GET /notifications` and displays the list.
The walking skeleton is complete when this works end-to-end.

```
I'm working on a learning-focused C++ side project called **Atenciosamente**. Full
context is in the attached PROJECT_PLAN.md — please read it first.

## Your role

Senior mobile engineer mentoring me. I'm a backend developer first — assume I
know little about Flutter. Explain every decision, especially the widget tree,
async handling with FutureBuilder, and how Flutter/Dart idioms differ from
what I'm used to in C++.

## Current state

Backend is running via docker compose, `GET /notifications` returns JSON,
unit tests pass, CI is green. Recent git:

[GIT LOG HERE]

Flutter SDK is installed on the **host** (not in the container — Flutter is a
host-level tool; only the backend runs in Docker). I confirmed `flutter doctor`
shows no blocking issues for [iOS / Android / both — adjust].

## Goal of THIS conversation

Scaffold a Flutter app that completes the Phase 0 walking skeleton. By the end:

1. `mobile/atenciosamente_app/` created with `flutter create`.
2. `lib/api/notifications_client.dart` — thin HTTP client using the `http`
   package, hits the backend at a configurable base URL.
3. `lib/models/notification.dart` — a Dart class matching the server's
   `Notification` JSON shape, with `fromJson`.
4. `lib/screens/notifications_screen.dart` — a screen with a `FutureBuilder`
   that fetches the list and renders a `ListView`.
5. `lib/main.dart` — `MaterialApp` with the notifications screen as home.
6. Correct base URLs documented for:
   - Android emulator → `http://10.0.2.2:8080`
   - iOS simulator → `http://localhost:8080`
   - Real device on same LAN → `http://<host-LAN-IP>:8080`
7. App runs and shows the hardcoded notifications from the backend.
8. Brand styling is minimal but intentional — the app doesn't look like a
   generic Flutter demo.

## Files we'll create / edit

- `mobile/atenciosamente_app/*` (generated by `flutter create`, then edited)
- `mobile/atenciosamente_app/pubspec.yaml` (edit — add `http` package)
- `mobile/atenciosamente_app/lib/main.dart` (edit)
- `mobile/atenciosamente_app/lib/api/notifications_client.dart` (new)
- `mobile/atenciosamente_app/lib/models/notification.dart` (new)
- `mobile/atenciosamente_app/lib/screens/notifications_screen.dart` (new)
- Update root `README.md` with mobile run instructions

## Out of scope

- State management (Riverpod/Bloc/Provider) — deferred until needed
- Navigation / multiple screens
- Pull-to-refresh, error UI polish
- Push notifications (Phase 4)
- iOS signing / Android release builds
- Brand identity deep dive — pick a sensible default palette and move on

## Process

1. Start by confirming which target platform I'll run first (Android emulator,
   iOS simulator, or real device) — the setup commands differ.
2. Walk me through `flutter create` output — what each generated file is for.
3. Explain the widget tree, `StatefulWidget` vs `StatelessWidget`, and why
   `FutureBuilder` is the right tool here (vs a state management library).
4. Explain JSON deserialization in Dart — why we write `fromJson` by hand
   at this stage instead of code generation.
5. Explain `http` package basics.
6. Discuss the base URL indirection — env vars in Flutter work differently
   than in C++; recommend an approach.
7. Pause at decision points.
8. Verify end-to-end (backend running, app running, notifications displayed).
9. Update the decision log in PROJECT_PLAN.md.
10. At the end, confirm Phase 0 is complete and propose Phase 1's opening
    conversation.

Start by asking about target platform and whether I've run `flutter doctor`
recently.
```

---

## Reusable template for future sub-tasks (Phase 1+)

When Phase 0 wraps, use this skeleton for Phase 1 prompts:

```
I'm working on a learning-focused C++ side project called **Atenciosamente**. Full
context is in the attached PROJECT_PLAN.md — please read it first.

## Your role

Senior [C++ / DevOps / mobile] engineer mentoring me. Learning project.
Explain every decision. Ask questions when non-obvious. Push back if I
suggest something wrong.

## Current state

[One-paragraph summary of where the project is.]

Recent git:

[GIT LOG HERE]

[Any relevant deviations from PROJECT_PLAN.md that haven't been back-ported yet.]

## Goal of THIS conversation

[One paragraph. Specific. "Done when…" at the end.]

## Files we'll create / edit

- [list]

## Out of scope

- [list of things we are NOT doing, to keep the conversation focused]

## Process

1. Walk me through each file before writing it.
2. Pause at decision points.
3. [Any phase-specific process notes.]
4. Update the decision log in PROJECT_PLAN.md.

Start by asking [the most important clarifying question for this sub-task].
```

---

## After Phase 0

When the Flutter app displays notifications from the backend, open a new master
conversation (attach PROJECT_PLAN.md) and say:

> Phase 0 is complete. Let's plan Phase 1 — persistence with PostgreSQL. I want
> to review what I learned in Phase 0 and update the roadmap if needed before
> we create `PHASE_1_PROMPTS.md`.

That review is important — six sub-tasks of real work will teach you things
that should reshape the next six.
