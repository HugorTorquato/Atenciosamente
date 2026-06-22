# Atenciosamente

> A mobile app that sends thoughtful notifications. Side project, learning-focused.

## What this is

**Atenciosamente** is a two-part project:

- A **Flutter** mobile app (iOS + Android) that displays notifications.
- A **C++ (Crow + PostgreSQL)** backend that serves and schedules them.

The project is primarily a learning exercise. Choices favor educational depth
alongside pragmatism — see [`PROJECT_PLAN.md`](./Documentation/PROJECT_PLAN.md) for the full
reasoning behind every decision.

## Status

🚧 **Phase 0 — Walking skeleton.** Not yet usable.

See the [roadmap in `PROJECT_PLAN.md`](./Documentation/PROJECT_PLAN.md#6-roadmap)
for what's coming.

## Repository layout

```
atenciosamente/
├── backend/         # C++ / Crow HTTP server
├── mobile/          # Flutter app
├── docker-compose.yml
└── Documentation/   # PROJECT_PLAN.md ← start here, plus concept/setup notes
```

## Requirements

- **Docker** & **Docker Compose** (v2) — the backend runs entirely in containers.
- **VS Code** + the **Dev Containers** extension (recommended editor setup).
- **Flutter SDK** — only needed to work on the mobile app. Install on the host,
  not in the container. See [flutter.dev](https://flutter.dev/docs/get-started/install).
- **WSL2** (if on Windows). Clone this repo *inside* WSL, not on `/mnt/c/...`.

## Running the backend (once Phase 0 lands)

```bash
# from the repo root
docker compose up --build

# the server will listen on http://localhost:8080
curl http://localhost:8080/notifications
```

## Running the mobile app (once Phase 0 lands)

**Physical device (same Wi-Fi as your dev machine):**

```bash
# from the repo root — auto-detects your Windows LAN IP
./run_dev.sh
```

**Android emulator:**

```bash
cd mobile/atenciosamente_app
flutter run --dart-define=API_BASE_URL=http://10.0.2.2:8080
```

> `10.0.2.2` is the emulator's alias for the host machine.

**Without a flag (CI or quick local test):**

```bash
cd mobile/atenciosamente_app
flutter run
# falls back to http://localhost:8080
```

## Learning notes

This repo is meant to be read as much as run. Notable files for understanding
the reasoning:

- [`Documentation/PROJECT_PLAN.md`](./Documentation/PROJECT_PLAN.md) — architecture, decision log, roadmap.
- `backend/CMakeLists.txt` — modern CMake walkthrough (once it exists).
- `backend/src/app.cpp` — how Crow wires up routes.
- `.github/workflows/backend-ci.yml` — test pyramid structure.

## License

TBD.
