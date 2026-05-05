# GitHub Actions — Field Guide

Context for the Atenciosamente backend CI pipeline.
Workflow file: `.github/workflows/backend-ci.yml` (at the repo root).

---

## Mental model: what GitHub Actions actually is

A workflow is a YAML file that describes **when** to run, and **what** to do.
GitHub watches your repo and spawns a fresh virtual machine (the "runner") each
time a trigger fires. The runner checks out your code and executes the steps
you defined, then shuts down. Nothing persists between runs except what you
explicitly cache.

```
trigger fires
    → GitHub provisions a fresh ubuntu-24.04 VM (~5s)
    → runner executes your steps top to bottom
    → runner exits; VM is discarded
```

---

## Anatomy of the workflow file

### `on:` — triggers

```yaml
on:
  push:
    branches: [main]
    paths:
      - 'backend/**'
      - '.github/workflows/backend-ci.yml'
  pull_request:
    branches: [main]
    paths: [same]
```

**`push`**: fires when a commit lands on `main` directly.
**`pull_request`**: fires when a PR is opened or updated against `main`.
**`paths:`**: the workflow only runs when at least one changed file matches.
A Flutter commit won't trigger a backend build. A docs-only commit won't either.

Without a `paths:` filter every commit to the repo — regardless of which
directory it touches — would trigger a backend CI build. That wastes minutes
and cache quota.

---

### `concurrency:` — cancel superseded runs

```yaml
concurrency:
  group: ${{ github.workflow }}-${{ github.head_ref || github.ref }}
  cancel-in-progress: true
```

If you push two commits to a PR branch quickly, the first run is cancelled
when the second starts. Without this you'd have two jobs competing for the
vcpkg cache and the status check would reflect whichever job happened to
finish last — not necessarily the latest code.

`head_ref` is the PR branch name (stable across force-pushes to that branch).
`ref` is the fallback for push events (`refs/heads/main`). The `||` picks
whichever is set.

---

### `permissions:` — least privilege

```yaml
permissions:
  contents: read
```

By default GitHub Actions grants the job's `GITHUB_TOKEN` broad permissions
(write to issues, PRs, packages). Restricting to `contents: read` means a
compromised step (e.g. a malicious action or a supply-chain attack) cannot
push code, open PRs, or publish packages on your behalf.

**The vcpkg binary cache is NOT controlled by `GITHUB_TOKEN`.** It uses
`ACTIONS_RUNTIME_TOKEN` — a separate, job-scoped credential that GitHub
injects automatically. No extra permission entry is needed for it.

---

### `env:` at the job level

```yaml
env:
  VCPKG_ROOT: /opt/vcpkg
  VCPKG_BINARY_SOURCES: "clear;x-gha,readwrite"
```

`VCPKG_ROOT` — where vcpkg is installed. The CMakePresets.json toolchain path
uses `$env{VCPKG_ROOT}` so both devcontainer and CI read from the same env var.

`VCPKG_BINARY_SOURCES` — tells vcpkg to use the GitHub Actions cache API as
its binary store. Breakdown:
- `clear` — reset any other sources (prevents inheriting system config)
- `x-gha` — use the GitHub Actions cache backend
- `readwrite` — both read restored binaries and write newly built ones

---

## The steps, one by one

### 1. Checkout — `actions/checkout@v4`

Clones the repo into the runner workspace. Without this, no step can read
your files.

### 2. Install build tools

Ubuntu 24.04 runners come with: `cmake 3.28`, `gcc-13`, `git`, `curl`,
`python3`, `docker`. What we install on top:

| Package | Why |
|---------|-----|
| `ninja-build` | CMake generator (faster than make) |
| `pkg-config` | vcpkg dependency resolution |
| `libssl-dev` | OpenSSL headers, needed by libpq and Crow |
| `zip unzip` | vcpkg package archive handling |
| `bison flex` | needed by libpq's build system on a cold vcpkg run |
| `autoconf automake libtool` | needed by some C library ports in vcpkg |

We also run `update-alternatives` to pin `gcc`/`g++` to version 13.
The runner may have 12, 13, and 14 installed; making 13 explicit ensures
CMake finds the same compiler as the Dockerfile.

### 3. Install vcpkg

```bash
sudo git clone --depth 1 https://github.com/microsoft/vcpkg /opt/vcpkg
sudo /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics
sudo chown -R "$USER" /opt/vcpkg
```

`--depth 1`: only the latest commit. Cloning takes ~25–35 seconds.
`chown -R "$USER"`: transfers ownership to the runner account so it can write
to vcpkg's cache directories. Prefer `chown` over `chmod -R a+rw` — the latter
gives world-write access to all files, which is unnecessarily permissive even
on an ephemeral runner.

**This step is intentionally not cached.** The vcpkg *tool* clone is cheap
(30s). The expensive part — compiled package binaries — is handled by
`VCPKG_BINARY_SOURCES`, which stores and restores those binaries separately.
Caching the tool clone itself would add key management complexity for
marginal benefit.

### 4. Configure — `cmake --preset ci`

Runs CMake with the `ci` preset:
- `CMAKE_BUILD_TYPE=Release`
- `BUILD_TESTING=ON`
- `ENABLE_SANITIZERS` not set (defaults OFF)

**This is where vcpkg installs packages.** CMake invokes vcpkg during
configure, which reads `vcpkg.json` and either:
- Restores pre-built binaries from the GHA cache (warm run, ~5s per package)
- Compiles from source and writes to the cache (cold run, ~3 min total)

### 5. Build — `cmake --build --preset ci`

Compiles `atenciosamente_core`, `atenciosamente_server`, and `tests_unit`.

### 6. Test — `ctest --preset ci --output-junit test-results.xml`

Runs all tests registered with CTest. Exit code is non-zero if any test
fails, which fails the step, which fails the job, which blocks the merge
(once branch protection is configured).

`--output-junit` writes a JUnit XML file — a standard format that GitHub,
Allure, and most CI dashboards understand. The file is relative to the
working directory (`backend/test-results.xml`).

### 7. Upload test results — `actions/upload-artifact@v4`

```yaml
if: always()
```

Without `if: always()`, the upload step is skipped when the test step fails
(because the job is already in "failure" state). The `always()` condition
forces the step to run regardless, so the XML is always available for
inspection in the GitHub Actions UI.

---

## vcpkg binary cache — how the cache key works

You don't manage the cache key. vcpkg computes it internally from:
- Package name + version (from `vcpkg.json` + baseline)
- Compiler ABI hash (compiler version + flags)
- Feature flags

This means:
- Updating `vcpkg.json` to add a new package → cold build for that package only
- Bumping the vcpkg baseline → cold builds for any packages that changed
- Changing `gcc-13` to `gcc-14` → full cold build (ABI mismatch)

The GHA cache has a **10 GB per repo limit** and entries expire after **7 days
of no access**. Current packages total ~200 MB.

---

## Branch protection — settings to enable in the GitHub UI

Path: **Settings → Branches → Add branch ruleset** (or "Add branch protection
rule" in older GitHub UI).

| Setting | Value | Why |
|---------|-------|-----|
| Branch name pattern | `main` | protects the integration branch |
| Require status checks before merging | ✓ | core rule |
| Status check to require | `Build and test (Release)` | this is the job's `name:` field |
| Require branches to be up to date | ✓ | prevents "merge window" races |
| Do not allow bypassing these settings | your call | disabling bypass means even admins are blocked |

**How to find the status check name**: push any commit to a branch and open a
PR. The checks panel shows the names. The name comes from the `name:` field of
the job, not the workflow — ours is `Build and test (Release)`.

---

## Adding Phase 1 — Postgres integration tests

Add a second job. Services run alongside the job container as a sidecar.

```yaml
  integration-test:
    name: Integration tests (Postgres)
    runs-on: ubuntu-24.04
    needs: build-and-test       # only run if unit tests pass

    services:
      postgres:
        image: postgres:17
        env:
          POSTGRES_USER: atenciosamente
          POSTGRES_PASSWORD: atenciosamente
          POSTGRES_DB: atenciosamente_test
        ports:
          - 5432:5432
        options: >-
          --health-cmd pg_isready
          --health-interval 5s
          --health-timeout 3s
          --health-retries 5

    env:
      DATABASE_URL: postgresql://atenciosamente:atenciosamente@localhost:5432/atenciosamente_test
      VCPKG_ROOT: /opt/vcpkg
      VCPKG_BINARY_SOURCES: "clear;x-gha,readwrite"

    steps:
      # same install + vcpkg + configure/build steps
      # then run: ctest --preset ci -R integration  (filter by tag or path)
```

`needs: build-and-test` — integration tests only run if unit tests pass.
This keeps the feedback loop fast: unit failure is cheaper to detect.

---

## Adding Phase 2 — Functional tests (live server)

The runner already has Docker installed. Start the full stack with Compose:

```yaml
  functional-test:
    name: Functional tests (HTTP)
    runs-on: ubuntu-24.04
    needs: integration-test

    steps:
      - uses: actions/checkout@v4
      - name: Start stack
        run: docker compose -f docker-compose.ci.yml up -d --wait
      - name: Run functional tests
        run: ./tests/functional/run.sh
      - name: Tear down
        if: always()
        run: docker compose -f docker-compose.ci.yml down -v
```

You'll want a separate `docker-compose.ci.yml` that overrides the dev
Compose file (no volume mounts, no devcontainer service, pinned image tags).

---

## Adding a lint/format job

Add this as a parallel job (no `needs:` so it runs alongside build-and-test):

```yaml
  lint:
    name: Lint and format check
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - name: Install clang-format
        run: sudo apt-get install -y clang-format
      - name: Check formatting
        run: |
          find backend/src backend/tests -name '*.cpp' -o -name '*.hpp' \
            | xargs clang-format --dry-run --Werror
```

`--Werror` exits non-zero if any file would be reformatted. The job fails
without touching the source.

---

## Dependabot for vcpkg baseline updates (future)

When `vcpkg.json` gets a `builtin-baseline` field (pinning vcpkg's package
versions), add `.github/dependabot.yml`:

```yaml
version: 2
updates:
  - package-ecosystem: "github-actions"
    directory: "/"
    schedule:
      interval: "weekly"
```

This keeps action versions (`actions/checkout@v4` etc.) current automatically.
vcpkg baseline updates are manual for now.

---

## Common pitfalls

| Symptom | Cause | Fix |
|---------|-------|-----|
| Cold build every run | `VCPKG_BINARY_SOURCES` not set or wrong | Check env var spelling |
| `cmake --preset ci` fails: toolchain not found | `VCPKG_ROOT` not set or vcpkg install failed | Check Install vcpkg step logs |
| Test results not uploaded on failure | `if: always()` missing on upload step | Add it |
| Wrong job name in branch protection | Job `name:` field doesn't match | Copy from the checks panel on a PR |
| Workflow doesn't trigger on a PR | `paths:` filter doesn't match changed files | Check the filter against the actual changed paths |
| Fork PR fails to write cache | `ACTIONS_RUNTIME_TOKEN` is read-only for forks | Expected; first-run will be slow but won't error |
