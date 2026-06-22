# Project Structure — Backend and Frontend

This document explains every folder and file in the repository and why it exists.

---

## Repository root

```
~/projects/Atenciosamente/
├── README.md               ← project overview and quick-start commands
├── .gitignore              ← ignores build/, .env, vcpkg_installed/, .dart_tool/, etc.
├── .editorconfig           ← consistent indentation/line endings across editors
├── .env.example            ← committed template; real .env is gitignored
├── docker-compose.yml      ← dev stack: backend + postgres containers
├── run_dev.sh              ← detects Windows LAN IP and runs Flutter with --dart-define
├── .devcontainer/
│   └── devcontainer.json   ← VS Code Dev Containers config (opens inside backend container)
├── .github/
│   └── workflows/
│       └── backend-ci.yml  ← GitHub Actions: build + unit tests on every push
├── backend/                ← C++ server (see below)
├── mobile/                 ← Flutter app (see below)
└── Documentation/          ← architecture decisions, phase prompts, concept guides
```

---

## Backend — `backend/`

The C++ Crow HTTP server and its build infrastructure.

```
backend/
├── Dockerfile.dev          ← dev image: ubuntu:24.04 + gcc-13 + clang-18 + vcpkg
├── Dockerfile              ← multi-stage prod build (Phase 5+, not yet used)
├── CMakeLists.txt          ← top-level build definition
├── CMakePresets.json       ← named presets: dev (Debug+sanitizers), ci (Release)
├── vcpkg.json              ← dependency manifest: Crow, nlohmann/json, Catch2, libpqxx
├── .clang-format           ← Google style, 4-space indent
├── .clang-tidy             ← static analysis rules
│
├── src/
│   ├── main.cpp            ← entry point: creates App, calls run()
│   ├── app.hpp / app.cpp   ← registers all routes on the Crow server
│   └── handlers/
│       ├── notifications.hpp   ← declares handle_get_notifications()
│       └── notifications.cpp   ← implements the GET /notifications handler
│
├── include/
│   └── atenciosamente/
│       ├── notification.hpp        ← Notification struct (data only, no behaviour)
│       └── notification_json.hpp   ← to_json(), serialize_notifications() free functions
│
└── tests/
    ├── CMakeLists.txt          ← defines tests_unit target, links atenciosamente_core
    └── unit/
        └── notification_json_test.cpp  ← Catch2 tests for JSON serialization
```

### Key backend concepts

**`atenciosamente_core` static library** — `notifications.cpp`,
`notification_json.cpp`, and other business-logic `.cpp` files are compiled into
a static library. Both the server binary and the test binary link against it.
This avoids compiling the same source twice and ensures tests run the exact same
code as production.

**`src/` vs `include/`** — public headers (types and functions other files use)
live in `include/atenciosamente/`. Implementation details live in `src/`. The
convention mirrors what you'd find in a library.

**`handlers/`** — one handler per endpoint. Each handler is a free function
that takes a `crow::request` and returns a `crow::response`. Handlers are
registered in `app.cpp`, not in `main.cpp`, so the app object can be tested
without starting the server.

**`CMakePresets.json`**:
- `dev` preset: Debug build + AddressSanitizer + UndefinedBehaviorSanitizer.
  Run during local development to catch memory errors and UB.
- `ci` preset: Release build, no sanitizers. Run in GitHub Actions.

**Build workflow:**
```bash
# Inside the backend container (docker compose exec backend bash)
cmake --preset=dev          # configure
cmake --build --preset=dev  # compile
ctest --preset=dev          # run tests
```

---

## Frontend — `mobile/atenciosamente_app/`

The Flutter Android app.

```
mobile/
└── atenciosamente_app/
    │
    ├── pubspec.yaml        ← dependencies + app metadata (CMakeLists.txt analogue)
    ├── pubspec.lock        ← exact resolved versions (vcpkg builtin-baseline analogue)
    ├── analysis_options.yaml  ← Dart linter rules (.clang-tidy analogue)
    │
    ├── lib/                ← ALL Dart source code lives here
    │   ├── main.dart       ← entry point: void main() + root MaterialApp widget
    │   │
    │   ├── api/
    │   │   └── notifications_client.dart  ← HTTP client: fetchNotifications(); URL via --dart-define at run time
    │   │
    │   ├── models/
    │   │   └── notification.dart          ← Notification class + fromJson constructor
    │   │
    │   └── screens/
    │       └── notifications_screen.dart  ← FutureBuilder + ListView UI
    │
    ├── test/
    │   └── widget_test.dart  ← widget tests (Phase 0 placeholder)
    │
    └── android/              ← Android-specific native layer (rarely touched)
        ├── build.gradle.kts        ← top-level Gradle config
        ├── settings.gradle.kts     ← module declarations
        ├── gradle.properties       ← JVM memory settings for Gradle
        ├── gradlew / gradlew.bat   ← Gradle wrapper (called by Flutter, not by you)
        └── app/
            ├── build.gradle.kts        ← app-level build config: minSdk, targetSdk, appId
            └── src/
                └── main/
                    ├── AndroidManifest.xml           ← permissions, app name, activities
                    └── kotlin/br/com/atenciosamente/
                        └── atenciosamente_app/
                            └── MainActivity.kt       ← Android Activity hosting Flutter (never edited)
```

### Key frontend concepts

**`lib/` is everything** — Flutter only compiles Dart files inside `lib/`. The
`android/` directory is compiled separately by Gradle. You will spend 95% of
your time in `lib/`.

**Folder conventions inside `lib/`:**

| Folder | Contains | Grows with |
|---|---|---|
| `api/` | Functions that talk to the backend | One file per API resource |
| `models/` | Dart classes mirroring backend JSON shapes | One file per entity |
| `screens/` | Full-screen widgets (one per route) | One file per screen |
| `widgets/` | Reusable sub-widgets shared across screens | Added when duplication appears |

**`android/app/build.gradle.kts`** — the key fields:
```kotlin
minSdk = 21          // oldest Android version the app supports
targetSdk = 33       // Android 13 — what we tested against
applicationId = "br.com.atenciosamente.atenciosamente_app"  // Play Store ID
```

**`AndroidManifest.xml`** — declares what the app is allowed to do at the OS level.
The `INTERNET` permission we added is required for any HTTP call. Without it,
the OS silently blocks the connection.

**`MainActivity.kt`** — the single Android Activity that Flutter renders into.
It is 5 lines of boilerplate generated by `flutter create`. You will not touch
this until Phase 4 (push notifications via platform channels).

---

## Documentation — `Documentation/`

```
Documentation/
├── README.md                    ← index of every doc + how to use them
├── PROJECT_PLAN.md              ← master architecture doc and decision log
│                                   Attach this to every new conversation.
│
├── reference/                   ← maps of "what exists and where"
│   └── project_structure.md     ← this file
│
├── concepts/                    ← language / framework / tooling learning notes
│   ├── dart_concepts.md         ← Dart language guide for C++ developers
│   ├── flutter_ui_concepts.md   ← Widget tree, FutureBuilder, StatelessWidget
│   ├── docker.md                ← Docker / devcontainer commands and concepts
│   └── github_actions.md        ← CI: GitHub Actions workflow concepts
│
├── setup/                       ← reproducible install / scaffold steps
│   ├── flutter_sdk.md           ← Flutter SDK install, ADB setup, Android SDK install
│   └── flutter_create.md        ← flutter create command + pubspec + AndroidManifest
│
└── phase-prompts/               ← bootstrap prompts, one file per phase
    └── PHASE_0_PROMPTS.md       ← bootstrap prompts for each Phase 0 sub-task
```

**Rule from the working style:** `PROJECT_PLAN.md` is the handoff document —
attach it to every new Claude conversation. The other files are reference
material; use them when you need to recall a concept or reproduce a setup step.
`Documentation/README.md` indexes them all. The category layout is kept tidy by
the `organize-docs` skill (`.claude/skills/organize-docs/`).

---

## How the two layers communicate

```
┌─────────────────────────────────────────────────────────────┐
│  phone                                                      │
│  Flutter app                                                │
│  http.get('http://<windows-lan-ip>:8080/notifications')     │
└────────────────────────┬────────────────────────────────────┘
                         │  WiFi LAN (HTTP/JSON)
                         ▼
┌─────────────────────────────────────────────────────────────┐
│  WSL2 (Ubuntu 24.04 on Windows)                             │
│                                                             │
│  ┌─────────────────── Docker Compose ───────────────────┐   │
│  │  backend container  :8080  ←── Crow C++ server       │   │
│  │  postgres container :5432  ←── PostgreSQL            │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

The phone talks to the backend over the LAN using the Windows host's IP address.
Docker's port binding (`ports: ["8080:8080"]` in `docker-compose.yml`) forwards
connections from the WSL2 host into the backend container.

The IP is not hardcoded. `run_dev.sh` (repo root) calls PowerShell from within
WSL to detect the current LAN IP and passes it to Flutter via:
```
flutter run --dart-define=API_BASE_URL=http://<detected-ip>:8080
```
`notifications_client.dart` reads this with `String.fromEnvironment('API_BASE_URL')`,
falling back to `localhost:8080` when the flag is absent (CI, emulator).
