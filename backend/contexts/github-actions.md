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

Compiles `atenciosamente_core`, `atenciosamente_server`, `tests_unit`, and
`tests_integration` — with no target specified, this builds everything
`BUILD_TESTING=ON` enables, in both the `unit` and `integration` jobs alike.
Each job only *runs* its own binary's tests (see the `TEST_PREFIX` section
below), but both still compile the other binary too; that's a small amount of
redundant compute per job, not per-target skipped, and is why the vcpkg
binary cache (not something more surgical like target-scoped builds) is what
keeps CI fast.

### 6. Test — `ctest --preset ci -R '<prefix>' --output-junit <file>.xml`

Runs the subset of CTest-registered tests matching `-R` — see "The
`tests_unit` / `tests_integration` filtering problem" further down for why
this is filtered at all, instead of just `ctest --preset ci` with no filter.
Exit code is non-zero if any selected test fails, which fails the step,
which fails the job, which blocks the merge (once branch protection is
configured).

`--output-junit` writes a JUnit XML file — a standard format that GitHub,
Allure, and most CI dashboards understand. The file is relative to the
working directory (`backend/test-results-unit.xml` or
`backend/test-results-integration.xml`, one per job).

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
| Status checks to require | `Unit tests` **and** `Integration tests (Postgres)` | these are the two jobs' `name:` fields — both must be required, since they run as independent, unordered jobs (see below) |
| Require branches to be up to date | ✓ | prevents "merge window" races |
| Do not allow bypassing these settings | your call | disabling bypass means even admins are blocked |

**How to find the status check names**: push any commit to a branch and open
a PR. The checks panel shows the names. The name comes from each job's
`name:` field, not the workflow — ours are `Unit tests` and
`Integration tests (Postgres)`.

---

## Phase 1 — Postgres integration tests (as built)

Two independent jobs, `unit` and `integration`, each doing its own full
checkout → install tools → install vcpkg → configure → build → test. Neither
has a `needs:` on the other.

**Why independent instead of `needs: unit`?** It's defensible either way —
this project chose independent/parallel because:
- the vcpkg binary cache makes a second full build cheap once warm (seconds,
  not the ~3 minute cold build), so building twice isn't wasteful enough to
  justify passing build artifacts between jobs (its own upload/download
  complexity, and a source of drift between two different job filesystems);
- running in parallel gives the fastest total signal — a genuine integration
  bug shows up immediately, not delayed behind however long the unit job
  takes;
- the two tiers test unrelated concerns, so gating one behind the other's
  success doesn't add information, only latency.

```yaml
  integration:
    name: Integration tests (Postgres)
    runs-on: ubuntu-24.04

    services:
      db:
        image: postgres:16
        env:
          POSTGRES_USER: atenciosamente
          POSTGRES_PASSWORD: devpassword
          POSTGRES_DB: atenciosamente_dev
        ports:
          - 5432:5432
        options: >-
          --health-cmd "pg_isready -U atenciosamente -d atenciosamente_dev"
          --health-interval 5s
          --health-timeout 5s
          --health-retries 5

    env:
      POSTGRES_USER: atenciosamente
      POSTGRES_PASSWORD: devpassword
      POSTGRES_DB: atenciosamente_dev
      POSTGRES_HOST: localhost   # not "db" — see note below
      POSTGRES_PORT: "5432"
      VCPKG_ROOT: /opt/vcpkg
      VCPKG_BINARY_SOURCES: "clear;x-gha,readwrite"

    steps:
      # same install + vcpkg + configure/build steps as the unit job, plus
      # `postgresql-client` in the apt install list (see note below), then:
      #   - run: ./scripts/migrate.sh          (working-directory: backend)
      #   - run: ctest --preset ci -R '^integration/' --output-junit ...
```

Postgres version and env var names/values are pinned to match the rest of the
project exactly, not invented per-CI values:
- `postgres:16` — the same version pinned in the repo's `docker-compose.yml`.
- `POSTGRES_USER`/`POSTGRES_PASSWORD`/`POSTGRES_DB` values match
  `.env.example` verbatim (`atenciosamente` / `devpassword` /
  `atenciosamente_dev`), so `make_connection()` and `scripts/migrate.sh`
  behave identically to local dev.
- `POSTGRES_HOST=localhost` is the one deliberate difference from
  `.env.example`'s `POSTGRES_HOST=db`. `db` is the Compose service name, only
  resolvable inside the Compose network used for local dev. GitHub Actions
  `services:` containers are different: they run alongside the job and,
  because `ports: ["5432:5432"]` publishes the container port onto the
  runner's own network namespace, are reached at `localhost`.

`scripts/migrate.sh` applies `migrations/*.sql` against the `POSTGRES_*` env
vars above (or `$DATABASE_URL` if that's set instead), tracking what's
applied in a `schema_migrations` table (see `scripts/migrate.sh` and the
`migrate` subcommand of `scripts/dev.sh` for the same thing locally). It
needs `postgresql-client` for `psql`. The `ubuntu-24.04` runner image's
documented "Installed apt packages" list (a curated subset, separate from a
pre-installed full PostgreSQL 16 server the image also ships, disabled by
default) doesn't explicitly name `postgresql-client`, so this workflow
installs it explicitly in the integration job's "Install build tools" step
rather than assume it's already on `PATH` — a cheap, idempotent apt install
either way.

Because a health check is configured on the `services:` container, GitHub
blocks the job's first step until `pg_isready` reports healthy — no manual
"wait for Postgres" step is needed.

### The `tests_unit` / `tests_integration` filtering problem

`backend/tests/CMakeLists.txt` builds two separate CTest-discoverable
binaries (`tests_unit`, needs no DB; `tests_integration`, needs a live
Postgres). Without something distinguishing their registered CTest names, a
single job's `ctest -R <pattern>` can't select "only this binary's tests" —
the individual `TEST_CASE` names don't share a common prefix per binary.

**Chosen fix: `TEST_PREFIX` on `catch_discover_tests()`.**

```cmake
catch_discover_tests(tests_unit TEST_PREFIX "unit/")
catch_discover_tests(tests_integration TEST_PREFIX "integration/")
```

This namespaces every discovered test under `unit/…` or `integration/…`, so:
- the `unit` job runs `ctest --preset ci -R '^unit/' --output-junit test-results-unit.xml`
- the `integration` job runs `ctest --preset ci -R '^integration/' --output-junit test-results-integration.xml`

Each job uploads its own `test-results-unit.xml` / `test-results-integration.xml`
as a distinct `actions/upload-artifact@v4` artifact (`v4` requires unique
artifact names within a run — reusing the old shared `test-results` name
across two jobs would fail the upload in whichever job ran second).

The alternative considered was invoking the built binaries directly
(`./build/ci/tests/tests_unit`, `./build/ci/tests/tests_integration`) instead
of going through `ctest -R`. That was rejected here because it would mean
inventing a different (Catch2-native) path to JUnit output, breaking the
existing `ctest --output-junit` + `upload-artifact` pattern for no real
benefit — `TEST_PREFIX` gets the same isolation with a one-line change per
binary and zero change to how results are produced or uploaded.

---

## Adding Phase 2 — Functional tests (live server)

The runner already has Docker installed. Start the full stack with Compose:

```yaml
  functional-test:
    name: Functional tests (HTTP)
    runs-on: ubuntu-24.04
    needs: integration       # this job's id, per the S6 job split — not "integration-test"

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

Add this as a parallel job (no `needs:` so it runs alongside `unit`/`integration`):

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
