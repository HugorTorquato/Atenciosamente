# CI Setup — Conversation Context

Paste this file at the start of a new chat to pick up where we left off on
the GitHub Actions CI pipeline for the Atenciosamente backend.

---

## Project snapshot

**Repo:** C++ learning project — Crow HTTP server, Postgres (coming), Flutter (coming).
**Backend stack:** C++20, CMake 3.25+, vcpkg, Catch2, Ninja, Docker devcontainer.
**Repo layout:**
```
~/projects/Atenciosamente/
├── .github/
│   └── workflows/
│       └── backend-ci.yml        ← CI workflow (repo root, NOT inside backend/)
├── backend/                      ← mounted as /workspace in the devcontainer
│   ├── CMakeLists.txt
│   ├── CMakePresets.json
│   ├── Dockerfile.dev
│   ├── vcpkg.json
│   ├── src/
│   ├── tests/unit/
│   └── contexts/                 ← reference docs (this file lives here)
└── Documentation/
    └── PROJECT_PLAN.md
```

---

## What is done

### CMake
- `atenciosamente_core` STATIC library — shared between server and tests.
- `atenciosamente_server` executable links the core lib + Crow + libpqxx.
- `tests_unit` executable links the core lib + Catch2WithMain.
- `catch_discover_tests` registers each TEST_CASE as a separate CTest entry.
- `BUILD_TESTING` guard — tests excluded from production builds.
- `CMakePresets.json` uses `$env{VCPKG_ROOT}` (not hardcoded `/opt/vcpkg`).

### CI workflow (`.github/workflows/backend-ci.yml`)
- **Runner:** `ubuntu-24.04` (not a container — runner directly).
- **Triggers:** `push` and `pull_request` to `main`, filtered to `backend/**` and the workflow file itself.
- **Concurrency:** cancels superseded runs on the same branch.
- **Permissions:** `contents: read` only (least privilege).
- **Caching:** vcpkg binary cache via `VCPKG_BINARY_SOURCES=clear;x-gha,readwrite`. No cache key management — vcpkg computes it from package + compiler ABI.
- **Steps:** checkout → install build tools → install vcpkg → configure → build → test → upload JUnit XML.
- **vcpkg install path:** `/opt/vcpkg` (matches `VCPKG_ROOT` in Dockerfile.dev).

### Tests
- One unit test: `tests/unit/notification_json_test.cpp`.
- 5 sections: empty vector, required keys, values, order, ISO 8601 timestamp.
- ASan caught a dangling-reference bug during initial setup (fixed).

---

## Decisions made and why

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Runner vs container | Runner directly | Simpler; scales to Postgres services (Phase 1) and docker compose (Phase 2) without Docker-in-Docker |
| Caching strategy | vcpkg binary cache (x-gha) | Official vcpkg feature; key computed by vcpkg, not manually managed |
| vcpkg tool clone | Fresh `--depth 1` each run | ~30s, cheaper than cache key management for the tool itself |
| `permissions:` | `contents: read` | Least privilege; vcpkg cache uses `ACTIONS_RUNTIME_TOKEN`, not `GITHUB_TOKEN` |
| Test artifact upload | `if: always()` + JUnit XML | Results saved even on failure; standard format for dashboards |
| `chmod` vs `chown` | `chown -R "$USER"` | Transfers ownership without granting world-write access |

---

## Security findings (addressed)

| Finding | Severity | Status |
|---------|----------|--------|
| `chmod -R a+rw` on vcpkg | Low | Fixed → `chown -R "$USER"` |
| Actions pinned to floating tag (`@v4`) not SHA | Medium | Documented; pin with SHA before going public |
| vcpkg clone without integrity check | Informational | Acceptable for learning project; fix with `builtin-baseline` in vcpkg.json later |

### How to pin actions to SHA (do this before the repo goes public)

```bash
# run from your host terminal to get current SHAs
git ls-remote https://github.com/actions/checkout refs/tags/v4
git ls-remote https://github.com/actions/upload-artifact refs/tags/v4
```

Then in the workflow:
```yaml
uses: actions/checkout@<SHA>       # v4.x.x
uses: actions/upload-artifact@<SHA> # v4.x.x
```

---

## What is NOT done yet

| Item | Phase | Notes |
|------|-------|-------|
| Branch protection rule in GitHub UI | now | Require `Build and test (Release)` status check |
| Postgres integration-test job | Phase 1 | Add `services: postgres:` block + `needs: build-and-test` |
| Functional-test job (live server) | Phase 2 | Use `docker compose` on the runner |
| Lint/format job | any time | `clang-format --dry-run --Werror` |
| `dependabot.yml` for action updates | optional | Keeps `@v4` tags fresh automatically |
| `builtin-baseline` in vcpkg.json | optional | Pins vcpkg package versions reproducibly |
| Action SHA pinning | before public | See security findings above |

---

## The workflow file (for reference — create at repo root, not inside backend/)

```yaml
# Location: ~/projects/Atenciosamente/.github/workflows/backend-ci.yml

name: Backend CI

on:
  push:
    branches: [main]
    paths:
      - 'backend/**'
      - '.github/workflows/backend-ci.yml'
  pull_request:
    branches: [main]
    paths:
      - 'backend/**'
      - '.github/workflows/backend-ci.yml'

concurrency:
  group: ${{ github.workflow }}-${{ github.head_ref || github.ref }}
  cancel-in-progress: true

permissions:
  contents: read

jobs:
  build-and-test:
    name: Build and test (Release)
    runs-on: ubuntu-24.04

    env:
      VCPKG_ROOT: /opt/vcpkg
      VCPKG_BINARY_SOURCES: "clear;x-gha,readwrite"

    steps:
      - name: Checkout
        uses: actions/checkout@v4   # TODO: pin to SHA before repo goes public

      - name: Install build tools
        run: |
          sudo apt-get update -qq
          sudo apt-get install -y --no-install-recommends \
            ninja-build pkg-config libssl-dev zip unzip \
            bison flex autoconf automake libtool
          sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 100
          sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 100

      - name: Install vcpkg
        run: |
          sudo git clone --depth 1 https://github.com/microsoft/vcpkg $VCPKG_ROOT
          sudo $VCPKG_ROOT/bootstrap-vcpkg.sh -disableMetrics
          sudo chown -R "$USER" $VCPKG_ROOT

      - name: Configure
        run: cmake --preset ci
        working-directory: backend

      - name: Build
        run: cmake --build --preset ci
        working-directory: backend

      - name: Test
        run: ctest --preset ci --output-junit test-results.xml
        working-directory: backend

      - name: Upload test results
        if: always()
        uses: actions/upload-artifact@v4   # TODO: pin to SHA before repo goes public
        with:
          name: test-results
          path: backend/test-results.xml
          retention-days: 30
```

---

## Reference docs in backend/contexts/

| File | Contents |
|------|----------|
| `github-actions.md` | Full field guide: triggers, caching, concurrency, permissions, Phase 1/2 expansion snippets |
| `unit-testing-infrastructure.md` | CMake target graph, sanitizer rules, how to add new tests |
| `catch2-guide.md` | TEST_CASE/SECTION/REQUIRE, output format, common pitfalls |
| `cmake.md` | CMake concepts used in the project |
| `docker.md` | Docker/devcontainer commands |
