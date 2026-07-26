# Atenciosamente — house rules for any agent

> Always-on context. Skills and subagents inherit this — they do **not** repeat it.

## What this project is

A **learning-focused** side project. Two parts:

- **Backend — C++20 (Crow + PostgreSQL).** This is the *primary* thing being learned:
  modern CMake, vcpkg, Crow, libpqxx, Catch2, concurrency, design patterns.
- **Mobile — Flutter.** Deliberately *secondary*. Kept minimal so it stays out of the way;
  it exists to exercise the backend end-to-end, not to be a showcase.

The developer is a backend engineer, new to Flutter. Favor **educational depth**: explain
trade-offs, don't just produce code. This is not a delivery race.

## Where the repo is (read this before touching files)

The **canonical repo is here, in WSL2**: `~/projects/Atenciosamente`
(`\\wsl.localhost\Ubuntu\home\torquato\projects\Atenciosamente` from Windows),
GitHub remote `hugortorquato/Atenciosamente`. It has the full history, backend + mobile
code, CI, and all docs.

⚠️ The Windows folder `c:\Projetos\Atenciosamente` is a **stale, partial copy** (no remote,
docs-only, no code). Never do work there. If a Claude session opened there, stop and
re-open in the WSL repo. Writing to the WSL path from a Windows session needs the
**PowerShell / Write / Edit tools** — the Bash sandbox sees it as read-only.

## Repo map

Don't re-derive the layout — it's documented in
[`Documentation/reference/project_structure.md`](Documentation/reference/project_structure.md).
The master architecture doc + decision log is
[`Documentation/PROJECT_PLAN.md`](Documentation/PROJECT_PLAN.md) — **attach it to every new
conversation**, and back-port any decision you make into its decision-log table.

Current state: **Phase 0 (walking skeleton) is complete.** Next is **Phase 1 — Persistence**
(swap the hardcoded list for Postgres via libpqxx, add `POST /notifications`, SQL migration
for the `notifications` table, integration tests with per-test transaction rollback).

## Commit style (strict)

`Scope (Tag): summary` — a **single line**. No body, no trailers, no `Co-Authored-By`.
Examples: `Backend (Feat): add POST /notifications`, `Docs (Reorg): tidy Documentation`.

## Working style

- One focused task per conversation; **every task ends in a commit**.
- `PROJECT_PLAN.md` is the handoff document — the source of truth for decisions.
- Commit only docs **or** code that belong together; don't sweep unrelated files in.
- When explaining a flow/lifecycle (e.g. RAII, a request path, a build pipeline), offer a
  Mermaid diagram as an Artifact for a quick visual — don't persist it to a repo `.md` file
  unless asked to.

## Build & test quick reference

**Backend** (inside the dev container — `docker compose exec backend bash`):
```bash
cmake --preset=dev          # configure (Debug + ASan/UBSan)
cmake --build --preset=dev  # compile
ctest --preset=dev          # run tests
```
**Mobile** (on the host, Flutter SDK installed):
```bash
./run_dev.sh                # physical device on the LAN (auto-detects host IP)
# or, from mobile/atenciosamente_app:
flutter run                 # falls back to http://localhost:8080
```

## AI-dev setup in this repo

- **`.claude/agents/`** — domain subagents you delegate to: `backend`, `frontend`.
  They run in their own context and reach for the skills below.
- **`.claude/skills/`** — reusable procedures, callable directly (`/skill-name`) or by a
  subagent: `backend-add-endpoint`, `backend-add-migration`, `frontend-add-model`,
  `frontend-add-screen`, and `organize-docs`. Add a new skill the first time a task recurs.
