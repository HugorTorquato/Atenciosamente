
---

**File 3 — Compressed context for future Docker conversations:**

# Docker Setup — Compressed Context

Use this to brief a new conversation about the Docker configuration.

---

## Current state (as of 2026-04-23)

Phase 0, sub-task 1 complete. Docker dev environment is fully working.

## Repo location

WSL2 Ubuntu 24.04: `~/projects/Atenciosamente/`
(NOT on Windows filesystem — moved to WSL2 for I/O performance with CMake/vcpkg)

## Files created this phase

### `backend/Dockerfile.dev`
- Base: `ubuntu:24.04`
- Installed: `ca-certificates`, `build-essential`, `gcc-13`, `g++-13`, `cmake`, `ninja-build`,
  `git`, `curl`, `zip`, `unzip`, `pkg-config`, `libssl-dev`, `gdb`, `clang-format`,
  `clang-tidy`, `sudo`
- gcc-13/g++-13 set as default via `update-alternatives`
- Non-root user `dev` (UID/GID 1000) with passwordless sudo
  — requires `userdel ubuntu` first because Ubuntu 24.04 image ships `ubuntu` user at UID 1000
- vcpkg installed at `/opt/vcpkg` via `git clone --depth 1`, bootstrapped, owned by `dev`
- `VCPKG_ROOT=/opt/vcpkg` in environment; vcpkg on PATH
- `WORKDIR /workspace`; `CMD ["sleep", "infinity"]` (placeholder until real server exists)

### `docker-compose.yml`
- `backend` service: builds from `backend/Dockerfile.dev`, bind-mounts `./backend:/workspace`,
  exposes port 8080, reads `.env`, depends on `db` with `service_healthy` condition
- `db` service: `postgres:16`, credentials from env vars, port 5432,
  named volume `postgres_data`, healthcheck via `pg_isready`
- `$$VAR` double-dollar required inside healthcheck `test:` strings to prevent
  Compose from expanding them before the shell sees them

### `.devcontainer/devcontainer.json`
- Attaches VS Code to the `backend` Compose service
- `remoteUser: dev`, `workspaceFolder: /workspace`, `shutdownAction: none`
- Extensions: `ms-vscode.cpptools` (debugging only), `llvm-vs-code-extensions.vscode-clangd`
  (IntelliSense + formatting), `ms-vscode.cmake-tools`, `twxs.cmake`
- `C_Cpp.intelliSenseEngine: disabled` — prevents cpptools/clangd IntelliSense conflict
- clangd will show degraded mode until `compile_commands.json` exists (sub-task 2)

### `.env.example` / `.env`

POSTGRES_USER=atenciosamente
POSTGRES_PASSWORD=devpassword
POSTGRES_DB=atenciosamente_dev
POSTGRES_HOST=db
POSTGRES_PORT=5432

`.env` is gitignored. `.env.example` is committed.

## Verified working

- `docker compose up -d` → both containers healthy
- Backend container: gcc 13.3.0, cmake 3.28.3, ninja 1.11.1, vcpkg 2026-04-08, clang-format 18.1.3, gdb 15.1
- Postgres: `atenciosamente_dev` database exists, UTF8, accessible via `psql`
- Dev Container opens in VS Code, lands as `dev` user at `/workspace`

## What does NOT exist yet

- No C++ source code
- No `CMakeLists.txt`, `vcpkg.json`, `CMakePresets.json`
- No `compile_commands.json` (clangd limited until sub-task 2)
- No production `Dockerfile` (Phase 5+)
- No CI workflow (sub-task 5)

## Known gotchas

- After `usermod -aG docker $USER`, run `newgrp docker` in the same shell or
  open a new terminal — group change only takes effect on new sessions
- If Docker socket permission denied after reopen, run `newgrp docker`
- To fully reset WSL2 docker group: `wsl --terminate Ubuntu` from Windows PowerShell
EOF

