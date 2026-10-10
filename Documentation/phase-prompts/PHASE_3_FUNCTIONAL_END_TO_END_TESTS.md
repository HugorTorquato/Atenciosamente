# Phase 3 — Functional (end-to-end) tests, in Python

> The plan for adding the third rung of the test pyramid: a **pytest** suite that talks to
> the **real `atenciosamente_server` binary over real HTTP**, backed by the **real Postgres**,
> exactly the way the phone does — driven by **one script you run by hand**, not by CI.
> This is a **plan document**. It describes the work and gives you a ready-to-paste
> prompt per step. It does not implement anything itself.
>
> Companion docs: attach [`PROJECT_PLAN.md`](../PROJECT_PLAN.md) to every step
> (it's the master decision log); the repo map is in
> [`reference/project_structure.md`](../reference/project_structure.md).
>
> **Numbering note:** the roadmap in `PROJECT_PLAN.md` §6 says Phase 3 is "scheduled
> notifications" and Phase 4 is "users & access control" — and commit `6b67a1c` just
> renumbered it. This file's `PHASE_3_` prefix is a leftover placeholder. **Recommendation:
> rename it to something non-numeric** (e.g. `FUNCTIONAL_TEST_TIER.md`) rather than
> renumbering the roadmap a second time — this is cross-cutting test infrastructure that
> can land between any two feature phases. Decide this before S1; if you do renumber
> instead, update §6 and the closing line of `PHASE_2_CONCURRENCY.md` in the same commit.
>
> ⚠️ **Read [§8](#8-ci-is-deferred--and-the-phase-4-collision) before starting.**
> `PHASE_4_REVIEW_AND_ADJUSTMENTS.md` §S6 proposes a *different, conflicting* functional
> tier (in-process Catch2 + a CI job). You should not build both. §8 lays out the choice.

---

## 1. Goal & definition of done

**Goal:** prove the HTTP API works **from the outside**. A test suite that knows nothing
about Crow, handlers, routes or C++ sends real requests to a real running server and
asserts on status codes, headers and JSON bodies. This covers the layer no current test
touches: `main.cpp` → `setup_routes()` → `handle_*()` → pool → repository → Postgres →
back out as an HTTP response.

**Deliberately kept simple.** An earlier draft of this plan added a `PORT` env var, a
dedicated `atenciosamente_functional` database, a database-name guard, and CTest preset
filters. All four are gone. They solved problems this workflow doesn't have, and every one
of them was a layer you'd have to maintain and explain. What's left is the suite, one
script, and nothing else. See §3.2.

**Explicit non-goals:**
- **CI — deferred, not rejected.** The suite is manual-only for now. S2's script is
  written to be CI-callable unchanged and carries a `TODO(ci)` block with the exact
  wiring steps, so picking it up later is a copy-paste job rather than a redesign. See
  [§8](#8-ci-is-deferred--and-the-phase-4-collision).
- **Test-data management.** The suite **truncates the dev database before every test**, on
  purpose: each test starts from an empty table and nothing relies on leftover rows.
  Keeping dev data alive across runs is a separate, later concern.
- **Mobile E2E** (Flutter `integration_test` driving the real app). Different tool,
  different phase. The app stays the manual end-to-end check.
- **Load / performance testing.** The concurrency test in S4 checks *correctness* under
  parallel requests, not throughput.
- **ThreadSanitizer.** The right tool for data races, but it can't be combined with the
  ASan/UBSan `dev` preset. Note it as a follow-up; don't add it here.
- **Failure-mode tests** (Postgres down → 500, etc.). Worth having later, but they need
  process control over the DB — a bigger harness than this phase needs.
- **Python in production code.** This is test-only tooling. Nothing under `backend/src/`
  grows a Python dependency, and no Python ships anywhere.

Phase 3 is **done** when all of these are true:

- [ ] The dev container can create a Python virtualenv (`python3-venv` in
      `Dockerfile.dev`), and `.gitignore` covers `.venv/`, `__pycache__/`,
      `.pytest_cache/`.
- [ ] A `backend/tests/functional/` pytest suite exists that imports **nothing** from the
      project — it only knows URLs, methods and JSON. It is **not** a CMake target and
      **not** a CTest entry.
- [ ] Every test starts from an empty `notifications` table, via an `autouse` fixture that
      `TRUNCATE`s before each test — and a `TODO` in `conftest.py` records that this is
      only safe while the one database is local and disposable.
- [ ] `ctest --preset=dev` is **completely unchanged** — no preset filters, no new target,
      no new vcpkg dependency, nothing under `backend/src/` touched. The C++ build never
      learns that this tier exists.
- [ ] One script (`backend/scripts/functional.sh`) does the whole cycle: ensure venv →
      refuse to run if something already holds `:8080` → migrate → start server → wait for
      ready → run pytest → stop server. Reachable as `scripts/dev.sh functional`.
- [ ] That script carries a **`TODO(ci)` block** naming the exact steps to wire it into
      `backend-ci.yml` when CI comes back, and takes `--preset ci` + pass-through pytest
      args today so no change is needed then.
- [ ] The suite also runs **standalone** (`pytest tests/functional`) against any
      already-running server via `ATENCIOSAMENTE_BASE_URL`, so you can debug with the
      server in the foreground or under `gdb`.
- [ ] Tests cover the full `/notifications` contract: 201 create, GET round trip and
      ordering, every 400 branch, "a 400 writes nothing", UTF-8 round trip, 404/405.
- [ ] A concurrency test fires more parallel POSTs than the pool has connections and
      proves none are lost or duplicated (the Phase 2 pool, exercised for real).
- [ ] `PROJECT_PLAN.md` §8/§10, the `backend-add-test` skill, `project_structure.md` and
      `CLAUDE.md`'s quick reference are updated — **including** the explicit record that
      the tier is manual-only, that it clears the dev database, and that §8's CI diagram
      is knowingly unfulfilled.
- [ ] Every sub-task is committed.

**What you'll learn:** black-box vs white-box testing; why transaction-rollback isolation
stops working the moment a second process commits; pytest fixtures, scopes and `autouse`
(and how they map onto Catch2's `TEST_CASE_METHOD`); `requests` and why its missing default
timeout is a footgun; Python virtualenvs and PEP 668; `concurrent.futures.ThreadPoolExecutor`
and why the GIL doesn't stop I/O-bound concurrency; and process orchestration from bash
(background processes, `trap`, readiness polling).

---

## 2. Where we are → where Phase 3 lands

Today the tiers are:

| Tier | Binary | What it calls | Needs |
|---|---|---|---|
| Unit | `tests_unit` | `domain/` functions directly | nothing |
| Integration | `tests_integration` | `repository/` + `ConnectionPool` directly | Postgres |
| **Functional** | **none** | — | — |

So `handle_post_notification`, `handle_get_notifications` and `setup_routes` are never
executed by any test. Specifically, nothing currently checks:

- that the 400 branches really return 400 with the `{"error": ...}` body,
- that a successful POST returns **201** (not 200) and `Content-Type: application/json`,
- that the POST and GET routes are registered on the right methods,
- that a POST followed by a GET returns the same data,
- that the Phase 2 pool behaves under real concurrent HTTP traffic.

`app.hpp` already says `setup_routes()` is "called by main and by tests" — which is
currently false; no test calls it. This phase makes the *behaviour* it produces covered,
without needing that claim to become true.

**Already in place:**

| Already in place | Where |
|---|---|
| A real server binary built by every preset | `atenciosamente_server` in `backend/CMakeLists.txt` |
| An idempotent migration runner that honours `DATABASE_URL` / `POSTGRES_*` | `backend/scripts/migrate.sh` |
| `curl`, `psql` and `python3` (3.12.3) in the dev container | `backend/Dockerfile.dev` |
| Configurable pool size | `POSTGRES_POOL_SIZE`, read in `main.cpp` |
| Postgres and the backend port published to the WSL host | `ports:` in `docker-compose.yml` (`5432`, `8080`) |

**Verified gap:** `python3` is present in the container but `python3 -m venv` **fails** —
`python3-venv` is not installed (it's `3.12.3-0ubuntu2.1` in `noble-updates/universe`).
There is no system `pip` either, and Ubuntu 24.04 marks its Python install
*externally managed* (PEP 668), so `pip install` outside a venv is refused by design.
S1 fixes this; see the layer-caching warning in that step.

---

## 3. Design rationale

### 3.1 Why Python for this tier

This project exists to learn C++, so putting the functional tier in another language
deserves an explicit justification rather than a shrug.

**What Python buys here:**

- **"No cheating" becomes structural, not a rule you have to remember.** The whole point of
  this tier is to be an *outside* observer. A C++ test binary sitting in the same build
  tree can always be tempted to `#include "handlers/notifications.hpp"` and call
  `handle_post_notification()` directly — skipping `main.cpp`, route registration and the
  socket, which are exactly the parts this tier exists to cover. A Python suite *cannot*
  link your code. The boundary enforces itself.
- **Zero C++ build surface.** No `cpp-httplib` in `vcpkg.json`, no `tests_functional`
  CMake target, no `CMakePresets.json` filters to keep `ctest --preset=dev` server-free.
  `ctest` keeps its exact current meaning for free, instead of by configuration you have
  to maintain.
- **The cheapest possible edit loop for contract tests.** Adding a case is a six-line
  function with no recompile. For a tier whose job is "assert on 13 rows of a table of
  status codes", that matters more than type safety does.
- **It's the language the rest of the world pokes your API with.** `requests` is the same
  shape as `curl` and as the Flutter `http` package. Reading the test tells you what the
  client contract is.

**What it costs — be honest about these:**

- **A second toolchain in the repo.** A venv, a `requirements.txt`, and one more thing to
  reinstall after a container rebuild.
- **No single "run all the tests" command.** `ctest --preset=dev` runs two tiers;
  `scripts/dev.sh functional` runs the third. Nothing runs all three.
- **It teaches you Python, not C++.** With `PORT` cut (§3.2) there is now **no C++ in this
  phase at all**. If you'd rather the tier itself be a C++ exercise, the alternative is in
  [§8.3](#83-the-collision-you-must-resolve-before-s1).
- **Sanitizer findings arrive sideways.** The server runs under ASan/UBSan with the `dev`
  preset, but a report lands in *its* stderr, in another process, where no assertion sees
  it. S2's log grep is the workaround, and it's a workaround, not a clean mechanism.

### 3.2 What got cut, and why

Four pieces of the first draft are gone. Recorded here so a future session doesn't
helpfully re-add them.

| Cut | Why it existed | Why it's gone |
|---|---|---|
| **`PORT` env var in `main.cpp`** | So the functional server could sit on its own port beside a running dev server | With one shared database, two servers can't usefully coexist anyway — the suite would be truncating the table the dev server is serving. The honest model is *one server at a time*, so S2 just binds `8080` and refuses to start if it's taken. `PORT` still has a story for the deploy ladder (§7 rung 2+); it isn't this phase's job. |
| **Dedicated `atenciosamente_functional` DB** | So `TRUNCATE` couldn't touch dev data | Clearing dev data every run is an accepted trade: every test must start fresh and none may rely on earlier rows. Data management is a later, separate concern. |
| **DB-name guard in `conftest.py`** | To make the `TRUNCATE` refuse any DB not ending in `_functional` | Nothing left to guard — there is one local, disposable database. Replaced by a `TODO` at the same spot: **reinstate a guard before Phase 5 puts a database anywhere non-local.** |
| **CTest preset filters + `tests_functional` target** | To keep `ctest --preset=dev` from trying to run tests that need a server | Unnecessary once the tier isn't a CMake target at all. `ctest` is untouched by construction, not by configuration. |

What survived, and why none of it is ceremony:

- **The venv and `python3-venv`** — irreducible. pytest isn't installed, and PEP 668 blocks
  installing it outside a venv.
- **The `autouse` `TRUNCATE` fixture** — this *is* the "every test starts fresh" rule.
- **`ApiClient`'s baked-in timeouts** — ~15 lines, and the only thing that turns a pool
  deadlock in S4 into a failed test instead of a terminal that hangs forever. There's no
  CTest timeout here to save you.
- **`scripts/functional.sh`** — kept at your call. It makes the run one command, guarantees
  the server is always stopped (`trap`), and is where the CI TODO lives so the path back to
  CI is written down rather than remembered.

---

## 4. Concepts you'll meet (read once)

- **Black-box vs white-box.** Unit and integration tests are *white-box*: they `#include`
  your code and call functions. A functional test is *black-box*: it only knows the
  server's public contract (URL, method, JSON). Python makes that distinction physical
  rather than aspirational — see §3.1.

- **Why rollback isolation stops working here.** Integration tests never commit, so
  `pqxx::work`'s destructor rolls everything back. In a functional test, the **server**
  commits (`txn.commit()` in `handle_post_notification`), in **another process**, on
  **another connection**. The test can't roll back someone else's committed transaction.
  So this tier needs a different isolation strategy: **reset the database before each
  test** (`TRUNCATE notifications RESTART IDENTITY`). That is destructive by design, and
  the thing being destroyed is your dev data — accepted, see §3.2.

- **One server, one database.** Because the suite truncates the same database the dev
  server reads, a dev server must not be running during a functional run. S2 enforces this
  by refusing to start when something already answers on `:8080`. Same reason: don't run
  `ctest --preset=dev` and the functional suite at the same time — the `TRUNCATE` will yank
  rows out from under an integration test that is mid-transaction.

- **pytest fixtures, mapped onto Catch2.** A `@pytest.fixture` is a named setup value a
  test requests by naming it as a parameter — dependency injection rather than
  inheritance. `scope="session"` ≈ a `static`/once-per-run value; the default
  function scope ≈ `TEST_CASE_METHOD`'s constructor running per case. `autouse=True`
  means every test gets it without asking, which is how the per-test `TRUNCATE` attaches
  with no boilerplate in the tests themselves. A fixture that `yield`s runs the code
  after the `yield` as teardown; ours deliberately resets *before* and has no teardown,
  so a failed test leaves its rows in the DB for you to inspect.

- **`requests` has no default timeout.** Omit `timeout=` and a hung server hangs the suite
  forever — there is no CTest 60-second backstop here, because CTest isn't involved. That's
  why the suite wraps `requests.Session` in a small `ApiClient` that always passes
  `timeout=(connect, read)`. This is the single most common `requests` mistake; meeting it
  deliberately is better than meeting it at 2am.

- **Send raw bodies, not `json=`.** `session.post(..., json={...})` is convenient and
  useless here: several required cases send bodies `json.dumps` could never produce
  (`not json`, a bare `[]`). So `ApiClient.post()` takes a **string** and the valid cases
  call `json.dumps()` themselves. One code path, and the malformed cases stop being
  special.

- **Virtualenvs and PEP 668.** Ubuntu 24.04 marks the system Python *externally managed*:
  `pip install` outside a venv is refused, on purpose, so pip can't fight apt over
  `/usr/lib/python3`. The fix is a venv (`python3 -m venv`), which needs the
  `python3-venv` package — and that package is what provides `ensurepip`, so the venv
  gets its own `pip` and you never need a system one. Do **not** reach for
  `--break-system-packages`.

- **The GIL does not stop this concurrency test.** Python bytecode doesn't run in parallel,
  but `requests` releases the GIL while blocked on a socket. Eight threads each waiting on
  a response really do have eight requests in flight at the server simultaneously — which
  is all this test needs, since the contention being proven is in the *server's* pool.

- **Readiness, not just liveness.** Starting the server in the background (`&`) returns
  immediately, before Crow has bound the port or the pool has opened its connections.
  The script must **poll** (`curl GET /` until it answers) before running tests, and
  also check the process hasn't died (`kill -0 $PID`), so a crash at startup fails fast
  with the log instead of hanging for the whole timeout.

- **`trap ... EXIT`.** A bash hook that runs when the script exits for *any* reason:
  success, a failed test, `set -e` aborting, or Ctrl-C. It's how the script guarantees
  the background server is always stopped. It's RAII for shell scripts.

- **Graceful shutdown.** Crow installs handlers for `SIGINT` and `SIGTERM` by default, so
  `kill -TERM $PID` makes `app.run()` return normally, `main()` exits, and the
  `ConnectionPool` destructor closes its connections. No orphaned server, no leaked
  Postgres sessions.

---

## 5. Step-by-step plan

Each step is one focused conversation that ends in a commit, the same rhythm as
Phases 0–2. Do them in order; later steps depend on earlier ones.

> **How to use the prompts:** finish and commit the current step first. Open a new
> conversation **in the WSL repo**. Attach `PROJECT_PLAN.md` and this file. Replace
> `[GIT LOG HERE]` with the output of `git log --oneline -10`. Paste the step's prompt.

---

### S1 — Python toolchain, fixtures, smoke test

Create the tier's skeleton and prove it can reach a running server with the simplest
possible test: `GET /` returns `200 "hello"`.

- **Files:**
  - `backend/Dockerfile.dev`: add `python3-venv` to the apt block.
  - `.gitignore`: add `.venv/`, `__pycache__/`, `.pytest_cache/`.
  - `backend/tests/functional/requirements.txt` (new).
  - `backend/tests/functional/conftest.py` (new) — `base_url`, `db_url`, the autouse
    `clean_database` reset, the `api` client fixture and the `make_api` factory S4 needs.
  - `backend/tests/functional/test_smoke.py` (new).
- **Skill:** `backend-add-test`. It only knows the two Catch2 tiers today; S5 updates it.
- **Decide & record:**
  - **Python instead of a C++ HTTP client.** The §3.1 argument. Record it properly — this
    is the decision a future session will most want the reasoning for, and it reverses
    the natural default for this repo.
  - **Libraries.** Recommended: `pytest` + `requests` + `psycopg[binary]`. All three are
    pure-wheel installs (no compiler, no `libpq` headers). `psycopg` rather than shelling
    out to `psql` because it keeps the suite self-contained — it then also runs from the
    WSL host, where `psql` may not exist but `pip` does, against the published `5432`.
    Alternative worth knowing: `subprocess.run(["psql", ...])` drops a dependency and
    reuses exactly the tool `migrate.sh` uses, at the cost of parsing shell output.
  - **Out-of-process (real socket) vs in-process.** Recommended: out-of-process. It tests
    the real `main()` (env parsing, pool construction, port binding, `.multithreaded()`),
    it's the same binary the phone talks to, and it's the only option where the test
    *cannot* share memory with the server. Note that §8.3's alternative is precisely the
    in-process version, so record the trade-off rather than just the choice.
  - **Isolation: per-test `TRUNCATE` against the dev database, no dedicated DB, no name
    guard** — with the `TODO` noting a guard must return before Phase 5. This is the entry
    a future session is most likely to second-guess, so write down that it was a
    deliberate trade (fresh-start tests now, data management later), not an oversight.
  - **Venv location.** Recommended: `backend/tests/functional/.venv`, gitignored. It's on
    the bind mount so it survives container restarts. Caveat to note: a venv built inside
    the container hardcodes container paths, so it is **not** reusable from the WSL host —
    make a separate one there if you want the host workflow (§7.4).
  - **No `pytest.ini`, no markers** (for now). With ~14 tests, `-k ordering` does what
    `-m post` would, with zero config. Add markers the first time `-k` is genuinely
    awkward, not before.
- **⚠️ Layer-caching warning:** editing the apt block in `Dockerfile.dev` invalidates
  every layer after it, **including the vcpkg clone + bootstrap** — a slow rebuild.
  Recommended: unblock immediately with
  `sudo apt-get update && sudo apt-get install -y python3-venv` inside the running
  container (the `dev` user has NOPASSWD sudo), **and** commit the `Dockerfile.dev` edit
  in the same step so the next rebuild is reproducible. Don't append a second apt block
  at the end of the file just to protect the cache — a tidy Dockerfile is worth more than
  one rebuild.

```txt
# backend/tests/functional/requirements.txt
pytest~=8.0
requests~=2.32
psycopg[binary]~=3.2
```

```python
# backend/tests/functional/conftest.py (shape: confirm during the step)
import os

import psycopg
import pytest
import requests

DEFAULT_BASE_URL = "http://localhost:8080"


class ApiClient:
    # requests applies no default timeout, and there is no CTest backstop here:
    # without these a hung server hangs the whole suite indefinitely.
    TIMEOUT = (2, 5)  # (connect, read) seconds

    def __init__(self, base_url):
        self._base_url = base_url
        self._session = requests.Session()

    def get(self, path):
        return self._session.get(self._base_url + path, timeout=self.TIMEOUT)

    # body is a raw str, never a dict: several cases must send bodies json.dumps
    # could not produce ("not json", a bare "[]").
    def post(self, path, body):
        return self._session.post(
            self._base_url + path,
            data=body.encode("utf-8"),
            headers={"Content-Type": "application/json"},
            timeout=self.TIMEOUT,
        )

    def request(self, method, path):
        return self._session.request(method, self._base_url + path, timeout=self.TIMEOUT)


@pytest.fixture(scope="session")
def base_url():
    return os.environ.get("ATENCIOSAMENTE_BASE_URL", DEFAULT_BASE_URL)


@pytest.fixture(scope="session")
def db_url():
    # TODO(data-management): this suite TRUNCATEs the database named by POSTGRES_DB,
    # which today is the dev database, deliberately — every test starts from an empty
    # table and none may rely on rows an earlier test or a manual curl left behind.
    # That is only safe while the single database is local and disposable. Before
    # Phase 5 points this at anything non-local, reinstate a name guard here.
    return (
        f"postgresql://{os.environ['POSTGRES_USER']}:{os.environ['POSTGRES_PASSWORD']}"
        f"@{os.environ['POSTGRES_HOST']}:{os.environ['POSTGRES_PORT']}"
        f"/{os.environ['POSTGRES_DB']}"
    )


# autouse: every test starts from an empty table without asking. Reset-before with
# no teardown, so a failed test leaves its rows behind for you to inspect.
@pytest.fixture(autouse=True)
def clean_database(db_url):
    with psycopg.connect(db_url, autocommit=True) as conn:
        conn.execute("TRUNCATE notifications RESTART IDENTITY")


@pytest.fixture
def api(base_url):
    return ApiClient(base_url)


# S4's concurrency test needs one client per thread, so it asks for the factory
# rather than importing ApiClient out of conftest.
@pytest.fixture
def make_api(base_url):
    return lambda: ApiClient(base_url)
```

```python
# backend/tests/functional/test_smoke.py (shape)
def test_root_answers_hello(api):
    res = api.get("/")
    assert res.status_code == 200
    assert res.text == "hello"
```

**▶ Prompt to implement this step**
```
Use the backend subagent to implement Step S1 of Phase 3 (functional tests).
Attached: PROJECT_PLAN.md and PHASE_3_FUNCTIONAL_END_TO_END_TESTS.md. Recent history:
[GIT LOG HERE]

Goal: create the functional test tier skeleton, in Python. Add python3-venv to the
apt block in backend/Dockerfile.dev (python3 3.12.3 is already present but
`python3 -m venv` fails without it); add .venv/, __pycache__/ and .pytest_cache/ to
.gitignore; add backend/tests/functional/ with requirements.txt (pytest, requests,
psycopg[binary]), conftest.py following the sketch in this doc, and test_smoke.py
asserting GET / is 200 "hello".

conftest.py specifics: base_url from ATENCIOSAMENTE_BASE_URL defaulting to
http://localhost:8080; db_url built from the POSTGRES_* env vars with NO database-name
guard but WITH the TODO(data-management) comment from the doc explaining that
truncating the dev DB is deliberate and that a guard must return before Phase 5;
an autouse clean_database fixture running TRUNCATE notifications RESTART IDENTITY;
an ApiClient with baked-in (2,5) timeouts that posts RAW string bodies, not json=;
plus `api` and `make_api` fixtures. No pytest.ini and no markers — we'll use -k.

Do NOT touch vcpkg.json, CMakeLists.txt, CMakePresets.json or anything under
backend/src/ — this tier adds no C++ build surface at all, and `ctest --preset=dev`
must come out byte-for-byte unchanged in meaning.

Before writing code, walk me through: out-of-process vs in-process testing, why
rollback isolation can't work across two processes, pytest fixtures/scopes/autouse
mapped onto Catch2's TEST_CASE_METHOD, and PEP 668 / why a venv rather than
--break-system-packages. Confirm the three libraries with me before installing.
For the apt change, unblock the running container with sudo apt-get install first
and commit the Dockerfile edit for reproducibility — tell me it invalidates the
vcpkg layer on the next rebuild.
For this step, run the smoke test by hand against a server you start manually; the
script comes in S2.
When done: record the decisions in PROJECT_PLAN.md §10 and commit in
`Scope (Tag): summary` style (no body, no trailers).
```

---

### S2 — Orchestration script: `scripts/functional.sh`

One script that does the whole lifecycle, so "run the functional tests" is a single
command. Nothing automated calls it — its job is to make the manual run **one command and
impossible to get half-wrong**: no dev server left serving the database you're about to
truncate, no orphaned process after a Ctrl-C, no forgotten migration.

It's also where the **route back to CI is written down**. Keep it `--preset ci`-callable
with pass-through args and a non-zero exit on any failure, and put the `TODO(ci)` block at
the top so the next person (you, in a month) doesn't have to re-derive the wiring.

- **Files:** `backend/scripts/functional.sh` (new, `chmod +x`);
  `backend/scripts/dev.sh`: add a `functional` subcommand to `usage()`, the argument
  `case`, and the dispatch `case` (format → configure → build →
  `exec scripts/functional.sh --preset $PRESET`).
- **Skill:** none. It's fresh bash in the style of `migrate.sh`/`dev.sh`.
- **Decide & record:**
  - **Refuse to start when `:8080` is occupied.** Recommended: yes, and this is the
    replacement for the `PORT`/second-port idea (§3.2). The error message should say to
    stop the dev server, because "one server, one database" is the actual constraint.
  - **Pool size for the run.** Recommended: `POSTGRES_POOL_SIZE=2`, deliberately smaller
    than S4's thread count so pool contention is *guaranteed*, not luck.
  - **Venv bootstrapping.** Recommended: the script creates/installs on first run and
    skips when `.venv/bin/pytest` already exists. One command on a fresh container, no
    reinstall on every run, no separate "setup" step to remember.
  - **Fail on sanitizer reports in the server log.** Recommended: yes. Under the `dev`
    preset the server runs with ASan/UBSan, and a report in *its* stderr would otherwise
    fail nothing, because pytest is a different process. Grep for
    `ERROR: AddressSanitizer` and `runtime error:`. Don't grep for LeakSanitizer at exit;
    third-party statics make it noisy.

```bash
#!/usr/bin/env bash
#
# Runs the functional (black-box HTTP) test tier end to end:
#   1. ensure the Python venv exists
#   2. refuse to run if something is already serving on :8080
#   3. apply migrations
#   4. start a real atenciosamente_server in the background
#   5. wait until it answers GET /
#   6. run pytest against it
#   7. stop the server (always, via trap) and surface its log on failure
#
# NOTE: this suite TRUNCATEs the notifications table before every test, in the
# database POSTGRES_DB names — the dev database. Every test starts from an empty
# table on purpose; nothing may rely on rows left by an earlier test or by a
# manual curl. Dev data does not survive a run. See PROJECT_PLAN.md §10.
#
# TODO(ci): nothing calls this automatically yet — CI is deferred, not rejected
# (PROJECT_PLAN.md §8's `functional` job is knowingly unbuilt). This script is
# written to stay CI-callable with no changes: `--preset ci`, pass-through pytest
# args, non-zero exit on any failure. To wire it up later:
#   1. Clone the `integration` job in .github/workflows/backend-ci.yml.
#   2. Add python3-venv to its apt-get install step.
#   3. Replace its migrate + ctest steps with:
#        ./scripts/functional.sh --preset ci -- --junit-xml=test-results-functional.xml
#   4. Upload test-results-functional.xml and build/ci/functional-server.log as
#      artifacts with `if: always()`.
#   5. Update the backend diagram in .github/workflows/README.md in the same commit.
# No port juggling is needed on a runner: the job owns the whole machine, so :8080
# is free and the occupancy check below just passes.
#
# Usage: scripts/functional.sh [--preset dev|ci] [-- <extra pytest args>]
#   e.g. scripts/functional.sh -- -k post -vv
set -euo pipefail
cd "$(dirname "$0")/.."

PRESET="dev"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --preset) PRESET="$2"; shift 2 ;;
        --) shift; break ;;
        *) echo "Unknown argument: $1" >&2; exit 1 ;;
    esac
done
PYTEST_EXTRA=("$@")

SERVER_PORT="${SERVER_PORT:-8080}"
BASE_URL="http://localhost:${SERVER_PORT}"
SERVER_BIN="./build/${PRESET}/atenciosamente_server"
LOG_FILE="./build/${PRESET}/functional-server.log"
SUITE_DIR="tests/functional"
VENV="${SUITE_DIR}/.venv"

if [[ ! -x "$SERVER_BIN" ]]; then
    echo "$SERVER_BIN not found. Build first: cmake --build --preset=$PRESET" >&2
    exit 1
fi

# ── 1. Python venv ───────────────────────────────────────────────────────────
# Ubuntu 24.04's system Python is externally managed (PEP 668), so a venv is the
# only sanctioned place to install into. Created once; reused after that.
if [[ ! -x "${VENV}/bin/pytest" ]]; then
    echo "==> Creating venv at ${VENV}"
    python3 -m venv "$VENV"
    "${VENV}/bin/pip" install --quiet --upgrade pip
    "${VENV}/bin/pip" install --quiet -r "${SUITE_DIR}/requirements.txt"
fi

# ── 2. One server, one database ──────────────────────────────────────────────
# The suite truncates the table a dev server would be serving, so they must not
# run at once. Checked up front rather than letting the bind fail obscurely.
if curl -fsS "${BASE_URL}/" >/dev/null 2>&1; then
    echo "Something is already serving on :${SERVER_PORT}." >&2
    echo "These tests TRUNCATE the notifications table — stop your dev server first." >&2
    exit 1
fi

# ── 3. Migrate ───────────────────────────────────────────────────────────────
# Idempotent, so this is a no-op on an already-migrated database; it's here so a
# fresh `docker compose down -v` doesn't turn into a confusing SQL error.
scripts/migrate.sh

# ── 4. Start the server ──────────────────────────────────────────────────────
export POSTGRES_POOL_SIZE="${FUNCTIONAL_POOL_SIZE:-2}"
export ATENCIOSAMENTE_BASE_URL="$BASE_URL"

echo "==> Starting server on :${SERVER_PORT} (pool=${POSTGRES_POOL_SIZE}, log: ${LOG_FILE})"
"$SERVER_BIN" >"$LOG_FILE" 2>&1 &
SERVER_PID=$!

stop_server() {
    if kill -0 "$SERVER_PID" 2>/dev/null; then
        kill -TERM "$SERVER_PID"          # Crow handles SIGTERM → graceful stop
        wait "$SERVER_PID" 2>/dev/null || true
    fi
}
trap stop_server EXIT

# ── 5. Wait for readiness ────────────────────────────────────────────────────
ready=0
for _ in $(seq 1 50); do               # 50 × 0.2s = 10s budget
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        echo "Server exited during startup:" >&2
        cat "$LOG_FILE" >&2
        exit 1
    fi
    if curl -fsS "${BASE_URL}/" >/dev/null 2>&1; then
        ready=1
        break
    fi
    sleep 0.2
done
if [[ "$ready" != "1" ]]; then
    echo "Server never became ready on ${BASE_URL}" >&2
    cat "$LOG_FILE" >&2
    exit 1
fi

# ── 6. Run the tests ─────────────────────────────────────────────────────────
echo "==> Running functional tests (pytest)"
set +e
"${VENV}/bin/pytest" "$SUITE_DIR" "${PYTEST_EXTRA[@]}"
status=$?
set -e

# ── 7. Sanitizer reports from the *server* process ───────────────────────────
# pytest is a separate process, so nothing it asserts can see these.
if grep -q -E "ERROR: AddressSanitizer|runtime error:" "$LOG_FILE"; then
    echo "==> Sanitizer report in server log:" >&2
    cat "$LOG_FILE" >&2
    status=1
elif [[ $status -ne 0 ]]; then
    echo "==> Server log (last 200 lines):"
    tail -n 200 "$LOG_FILE"
fi

exit "$status"
```

**▶ Prompt to implement this step**
```
Use the backend subagent to implement Step S2 of Phase 3 (functional tests).
Attached: PROJECT_PLAN.md and PHASE_3_FUNCTIONAL_END_TO_END_TESTS.md. Recent history:
[GIT LOG HERE]

Goal: add backend/scripts/functional.sh following the sketch in this doc: create the
venv at tests/functional/.venv on first run and install requirements.txt into it;
refuse to run (with a clear message about stopping the dev server) if anything already
answers on :8080, because the suite TRUNCATEs the table a dev server would be serving;
run scripts/migrate.sh; start atenciosamente_server in the background with
POSTGRES_POOL_SIZE=2; poll GET / until ready, failing fast if the process dies; run
pytest with pass-through args after `--`; always stop the server via `trap ... EXIT`;
print the server log on failure; and fail if the server log contains an ASan/UBSan
report. Add a `functional` subcommand to scripts/dev.sh (usage text, arg case, dispatch).

Include the TODO(ci) comment block from the doc verbatim in spirit — the 5 numbered
steps for wiring this into backend-ci.yml later. CI is deferred, not rejected, and the
script must stay callable as `--preset ci` with pass-through args so adopting it needs
no redesign. Do not add any CI workflow file or job in this step.

Explain trap, background processes, wait and kill -0 as you go.
Prove it by running `scripts/dev.sh functional` (smoke test green); then break the
smoke assertion on purpose and confirm the script exits non-zero, prints the log, and
leaves no server running (`pgrep -a atenciosamente_server`); then confirm Ctrl-C
mid-run leaves no orphan; then start a server by hand and confirm the :8080 occupancy
check refuses with the intended message.
When done: record the port-reuse/pool/venv/sanitizer-grep decisions in PROJECT_PLAN.md
§10 and commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### S3 — The `/notifications` contract

The actual coverage. One test function per behaviour, so a failure names exactly which
part of the contract broke.

- **Files:** `backend/tests/functional/test_notifications_api.py` (new).
- **Skill:** `backend-add-test`.
- **Cases:**

| # | Request | Expect | Why it matters |
|---|---|---|---|
| 1 | `GET /notifications` on an empty DB | `200`, `[]`, `Content-Type: application/json` | Baseline, and proves the reset fixture works |
| 2 | `POST` valid `{title, body}` | `201`, JSON with `id > 0`, echoed `title`/`body`, `created_at` matching `^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$` | The happy path and the exact status code the app relies on |
| 3 | `POST` then `GET` | The GET array contains an element **equal** to the POST response | Round trip through the real DB |
| 4 | `POST` A, then `POST` B, then `GET` | B comes before A | `ORDER BY created_at DESC, id DESC`, seen from outside |
| 5 | `POST` body `not json` | `400`, `{"error":"request body must be valid JSON"}` | Parse-failure branch |
| 6 | `POST` `[]` (an array) | `400`, `"request body must be a JSON object"` | Non-object branch |
| 7 | `POST` `{"title":"x"}` | `400`, `"title and body are required"` | Missing-field branch |
| 8 | `POST` `{"title":1,"body":"x"}` | `400`, `"title and body must be strings"` | Type branch |
| 9 | `POST` `{"title":"","body":"x"}` | `400`, `"title and body must not be empty"` | Empty branch |
| 10 | Any 400 above, then `GET` | still `[]` | **A rejected request writes nothing** (only provable end to end) |
| 11 | `POST` `"Consulta às 10h — não esqueça"` | Same characters come back on POST and GET | UTF-8 survives Crow → libpqxx → Postgres → nlohmann → the wire |
| 12 | `GET /does-not-exist` | `404` | Router fallthrough |
| 13 | `PUT /notifications` | `405` | Method wiring. **Verify** Crow's actual behaviour in this step — both routes are registered on the same path, so it may answer `404`; if it differs, assert what it really returns and record it |

Assert error bodies via `res.json()["error"]`, not by comparing raw text, so key order or
whitespace can't cause false failures. Cases 5–9 are the same messages the
`create_notification_request` unit tests check (`create_notification_request.cpp` is the
single source of those strings). That duplication is deliberate: the unit tests prove the
function *returns* them, the functional tests prove they *reach the wire* with a 400.

Use `@pytest.mark.parametrize` for cases 5–9 — they're one behaviour with five inputs, and
parametrize reports each as its own test, which is exactly the "a failure names which part
broke" property we want. Resist collapsing cases 1–4 into it; those are different
behaviours, not different inputs.

Every test gets an empty table from `clean_database`, so no test may assume a row another
test created — if a test needs data, it POSTs it itself.

```python
# shape: the parametrized validation block
import json
import pytest

INVALID_BODIES = [
    ("not json", "request body must be valid JSON"),
    ("[]", "request body must be a JSON object"),
    (json.dumps({"title": "x"}), "title and body are required"),
    (json.dumps({"title": 1, "body": "x"}), "title and body must be strings"),
    (json.dumps({"title": "", "body": "x"}), "title and body must not be empty"),
]


@pytest.mark.parametrize("body,expected_error", INVALID_BODIES)
def test_invalid_post_is_rejected_and_writes_nothing(api, body, expected_error):
    res = api.post("/notifications", body)
    assert res.status_code == 400
    assert res.json()["error"] == expected_error

    # The point of testing this end to end: no lower tier can prove the row
    # was never written, only that the function returned an error.
    assert api.get("/notifications").json() == []
```

**▶ Prompt to implement this step**
```
Use the backend subagent to implement Step S3 of Phase 3 (functional tests).
Attached: PROJECT_PLAN.md and PHASE_3_FUNCTIONAL_END_TO_END_TESTS.md. Recent history:
[GIT LOG HERE]

Goal: add tests/functional/test_notifications_api.py covering every row of the §5 S3
table in this doc (201 create, round trip, ordering, each 400 branch, "a 400 writes
nothing", UTF-8 round trip, 404, 405). One test function per behaviour, with
@pytest.mark.parametrize for the five 400 branches only. Assert error bodies via
res.json()["error"], never raw text. Take the `api` fixture; don't build your own
client. No test may depend on data another test created — clean_database truncates
before each one, so any test needing rows POSTs them itself.
For the 405 case: both GET and POST are registered on "/notifications" in app.cpp —
check what Crow actually returns for PUT before asserting, and tell me the answer.
Run with scripts/dev.sh functional, and also confirm `-k invalid` selects only the
five parametrized 400 cases.
When done: update PROJECT_PLAN.md §10 if anything was decided (e.g. Crow's real
405/404 behaviour) and commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### S4 — Concurrency under real HTTP (the Phase 2 pool, for real)

Phase 2's pool tests use threads calling `acquire()` directly. This step proves the same
property from outside: many simultaneous HTTP requests, more than the pool has
connections, and every one of them lands exactly once.

- **Files:** `backend/tests/functional/test_concurrency.py` (new).
- **Skill:** `backend-add-test`.
- **Shape:** the server runs with `POSTGRES_POOL_SIZE=2` (set by S2's script). Use
  `ThreadPoolExecutor(max_workers=8)`; each worker builds **its own** client via the
  `make_api` fixture (`requests.Session` is not thread-safe) and sends 10 POSTs with
  unique titles (`"t3-n7"`), returning its own list of results. No shared mutable state,
  so no lock in the test. After the executor drains:
  - all 80 statuses are `201`,
  - the 80 returned ids are unique (`len(set(ids)) == 80`),
  - one `GET /notifications` returns exactly 80 items, and the set of returned titles
    equals the set of sent titles.
- **Why the GIL doesn't invalidate it:** `requests` releases the GIL while blocked on the
  socket, so eight requests really are in flight at the server at once. The contention
  being proven is in the *server's* pool, not in Python.
- **Why it can't be flaky:** it asserts only on *outcomes* (counts, uniqueness, set
  equality), never on timing or interleaving. If the pool deadlocks, `ApiClient`'s 5s read
  timeout turns the hang into a failed test rather than a hung suite — note that this is
  now the **only** backstop, since there's no CTest timeout here.
- **Bonus under the `dev` preset:** the server is running under ASan/UBSan while it takes
  this load, and S2's log grep fails the run on any report. This is the most valuable
  thing in the phase and it comes for free.

```python
# shape
import json
from concurrent.futures import ThreadPoolExecutor

THREADS, PER_THREAD = 8, 10


def _post_batch(thread_index, make_api):
    # Its own client per thread: requests.Session is not thread-safe.
    api = make_api()
    results = []
    for n in range(PER_THREAD):
        title = f"t{thread_index}-n{n}"
        res = api.post("/notifications", json.dumps({"title": title, "body": "concurrent"}))
        results.append((title, res.status_code, res.json().get("id")))
    return results


def test_parallel_posts_all_land_exactly_once(api, make_api):
    with ThreadPoolExecutor(max_workers=THREADS) as pool:
        batches = pool.map(lambda i: _post_batch(i, make_api), range(THREADS))
    ...
```

**▶ Prompt to implement this step**
```
Use the backend subagent to implement Step S4 of Phase 3 (functional tests).
Attached: PROJECT_PLAN.md and PHASE_3_FUNCTIONAL_END_TO_END_TESTS.md. Recent history:
[GIT LOG HERE]

Goal: add tests/functional/test_concurrency.py: a ThreadPoolExecutor with 8 workers,
each building its own client via the make_api fixture (don't import ApiClient out of
conftest) and POSTing 10 notifications with unique titles, to a server whose pool
size is 2. Collect per-thread results with no shared mutable state, then assert all 80
are 201, all 80 ids are unique, and a single GET returns exactly those 80 titles.
Assert only on outcomes, never on timing.
Explain why one Session per thread, why the GIL doesn't make this a fake concurrency
test, and why the read timeout is now the only thing standing between a pool deadlock
and a hung suite.
Run it with scripts/dev.sh functional several times in a row; it must be green every
time, and the server log must stay free of sanitizer reports.
When done: commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### S5 — Docs sweep

Make the rest of the repo aware that a third tier exists — that it's manual, and that it
clears the dev database — so future sessions (and future you) don't re-derive it, assume
CI is covering it, or get surprised by an empty table.

- **Files:**
  - `Documentation/PROJECT_PLAN.md` §8: change the functional rung's "(Phase 2+)" to the
    real phase; note the per-test `TRUNCATE` and that it runs against the dev DB; and
    **explicitly mark the `functional` box in the CI diagram as deliberately not built
    yet**, pointing at `scripts/functional.sh`'s `TODO(ci)` for the wiring and §10 for
    the reasoning. A diagram promising a job that doesn't exist is worse than no diagram.
  - `.claude/skills/backend-add-test/SKILL.md`: fill in the "Functional/E2E" row of the
    §1 tier table (it currently reads "not built yet (Phase 2+)"), rewrite §4 ("No tier
    fits") now that one does, and add a "Functional test procedure" section: the pytest
    suite, the `api`/`clean_database` fixtures, no imports from the project, run with
    `scripts/dev.sh functional`. Keep §1's "lowest tier that can catch the bug" rule
    front and centre — the new tier must not become the default landing place, and it's
    the only tier with no CI safety net.
  - `Documentation/reference/project_structure.md`: `tests/functional/` (with its Python
    files), `scripts/functional.sh`, `python3-venv` in the Dockerfile.dev line, and the
    stale "build + unit tests" CI description.
  - `CLAUDE.md` quick reference: add `scripts/dev.sh functional`, note that
    `ctest --preset=dev` does **not** include it (nothing to configure — the tier simply
    isn't a CTest target), and warn that it clears the dev `notifications` table and must
    not run alongside a dev server or a `ctest` run.
- **Skill:** `organize-docs`, if it fits.

**▶ Prompt to implement this step**
```
Implement Step S5 of Phase 3 (functional tests): a docs sweep.
Attached: PROJECT_PLAN.md and PHASE_3_FUNCTIONAL_END_TO_END_TESTS.md. Recent history:
[GIT LOG HERE]

Goal: update PROJECT_PLAN.md §8, .claude/skills/backend-add-test/SKILL.md (fill the
functional tier row, rewrite the "no tier fits" section, add the pytest procedure),
Documentation/reference/project_structure.md, and CLAUDE.md's build & test quick
reference so they all reflect the new Python functional tier and scripts/functional.sh.
Three facts must land everywhere they're relevant: it is MANUAL (no CI job yet — point
at the TODO(ci) in scripts/functional.sh for how to add one), it TRUNCATES the dev
notifications table before every test, and it must not run alongside a dev server or a
ctest run. In §8's CI diagram, mark the functional box as deliberately not built yet
rather than leaving a diagram that promises a job nobody wrote. Docs only, no code.
When done: commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

## 6. Testing tiers after Phase 3

| | **Unit** | **Integration** | **Functional** |
|---|---|---|---|
| Lives in | `tests/unit/` | `tests/integration/` | `tests/functional/` |
| Language | C++ / Catch2 | C++ / Catch2 | **Python / pytest** |
| Binary | `tests_unit` | `tests_integration` | — (not a build target) |
| CTest prefix | `unit/` | `integration/` | — (not in CTest) |
| Calls | `domain/` functions | `repository/`, `ConnectionPool` | **HTTP only** |
| Links the project? | yes | yes | **no — can't** |
| Needs | nothing | Postgres | Postgres **+ running server** |
| Database | — | dev DB | dev DB (**same one**) |
| Isolation | none needed | per-test **rollback** (never commit) | per-test **TRUNCATE** (server commits) |
| Destroys dev data? | no | no | **yes, every run** |
| Run with | `ctest --preset=dev -R '^unit/'` | `ctest --preset=dev -R '^integration/'` | `scripts/dev.sh functional` |
| CI job | `unit` | `integration` | **none yet — see `TODO(ci)`** |

Two consequences of the functional tier sharing the dev database:

- **Don't run it alongside a dev server.** S2's script refuses to start if `:8080` is
  taken, for exactly this reason.
- **Don't run it alongside `ctest`.** The `TRUNCATE` will yank rows out from under an
  integration test that's mid-transaction. They're fine sequentially, not concurrently.

The rule from `backend-add-test` still holds: **the lowest tier that can catch the bug.**
Don't move validation tests up to functional because they now *can* run there — and the
pull will be strong, because Python is the pleasant one to write. The functional tier only
asserts what the lower tiers can't see: status codes, headers, routing, and the whole path
working together. It is also the only tier with no CI safety net, which is a second reason
not to let logic coverage drift into it.

---

## 7. Running the functional tests manually

All commands run **inside the backend dev container** unless marked otherwise.

### 7.1 One-time setup (after S1 lands)

```bash
# on the WSL host, from the repo root
docker compose up -d                 # db (health-checked) + backend container
docker compose exec backend bash     # shell inside the dev container

# inside the container (/workspace == backend/)
cmake --preset=dev                   # configure
cmake --build --preset=dev           # builds the server and both C++ test binaries
```

The Python venv is created by `scripts/functional.sh` on its first run — there's no
separate setup step. If your container predates S1's `Dockerfile.dev` change and you
haven't rebuilt, `python3 -m venv` fails with "ensurepip is not available"; fix it with
`sudo apt-get update && sudo apt-get install -y python3-venv`, or rebuild the image.

### 7.2 The normal way: one command

```bash
scripts/dev.sh functional            # format → configure → build → functional.sh
# or, if already built:
scripts/functional.sh
```

What you should see:

```
==> Creating venv at tests/functional/.venv          (first run only)
==> Database up to date
==> Starting server on :8080 (pool=2, log: ./build/dev/functional-server.log)
==> Running functional tests (pytest)
================== test session starts ==================
tests/functional/test_concurrency.py .
tests/functional/test_notifications_api.py ............
tests/functional/test_smoke.py .
================== 14 passed in 3.41s ===================
```

(14 = 1 smoke + 1 concurrency + 12 contract tests, since S3's five 400 branches are one
parametrized function reported as five cases.)

Exit code `0` = green. Non-zero = a test failed, the server crashed or never became
ready, `:8080` was occupied, or the server logged a sanitizer report. The script prints
the server log in every failure case.

**Your `notifications` table is empty afterwards.** That's the design, not a bug — if you
want manual data back for the phone, POST it again with curl.

**Run a subset** (everything after `--` goes to pytest):

```bash
scripts/functional.sh -- -k ordering -vv       # one test by name substring, verbose
scripts/functional.sh -- -k invalid            # the parametrized 400 branches
scripts/functional.sh -- -x --lf               # stop at first failure; rerun last failures
```

### 7.3 The step-by-step way (what the script does, by hand)

Do this once to understand the moving parts, and whenever you need to debug with the
server in the foreground or under `gdb`. This is also the shape you'd use if you prefer
driving it yourself and skipping the script entirely.

```bash
# 0. Make sure no other server is up — it would be serving the table you're about
#    to truncate, and it would hold :8080.
pgrep -a atenciosamente_server && pkill -TERM atenciosamente_server

# 1. Migrate (idempotent; a no-op unless the schema changed)
scripts/migrate.sh

# 2. Start the server (background here, or foreground in a 2nd terminal)
POSTGRES_POOL_SIZE=2 ./build/dev/atenciosamente_server &
#   second terminal: docker compose exec backend bash, then
#   POSTGRES_POOL_SIZE=2 gdb --args ./build/dev/atenciosamente_server

# 3. Poke it by hand
curl -i http://localhost:8080/
curl -i -X POST http://localhost:8080/notifications \
     -H 'Content-Type: application/json' \
     -d '{"title":"Lembrete","body":"Beba água"}'
curl -i http://localhost:8080/notifications
curl -i -X POST http://localhost:8080/notifications -d 'not json'    # expect 400

# 4. Run the tests against it (this clears the table first, per test)
tests/functional/.venv/bin/pytest tests/functional              # everything
tests/functional/.venv/bin/pytest tests/functional -k utf8 -vv  # one test, verbose
#   or `source tests/functional/.venv/bin/activate` once and just type `pytest`

# 5. Stop the server
kill %1      # or: pkill -TERM atenciosamente_server
```

`ATENCIOSAMENTE_BASE_URL` defaults to `http://localhost:8080`, so step 4 needs no env var
as long as the server is on the default port. Set it if you moved the server.

### 7.4 Optional: run pytest from the WSL host

`docker-compose.yml` publishes both `5432` and `8080`, so the suite can run outside the
container entirely — useful if you prefer your host editor's Python tooling. The **server
still has to run in the container** (that's where it's built), and you need a *separate*
venv, because the container's `.venv` hardcodes container paths.

```bash
# on the WSL host, from backend/
python3 -m venv ~/.venvs/atenciosamente-functional        # apt install python3-venv if needed
~/.venvs/atenciosamente-functional/bin/pip install -r tests/functional/requirements.txt

# POSTGRES_HOST is localhost here, not the compose network name `db`
export POSTGRES_HOST=localhost POSTGRES_PORT=5432
export POSTGRES_USER=atenciosamente POSTGRES_PASSWORD=devpassword
export POSTGRES_DB=atenciosamente_dev
~/.venvs/atenciosamente-functional/bin/pytest tests/functional
```

### 7.5 Housekeeping & troubleshooting

| Symptom | Cause / fix |
|---|---|
| `Something is already serving on :8080` | A dev server is up. Stop it (`pkill -TERM atenciosamente_server`) — the suite truncates the table it's serving, so they can't share. |
| `ensurepip is not available` | `python3-venv` missing — pre-S1 container. `sudo apt-get update && sudo apt-get install -y python3-venv`, or rebuild the image. |
| `error: externally-managed-environment` | You ran `pip install` outside the venv (PEP 668). Use `tests/functional/.venv/bin/pip`, never `--break-system-packages`. |
| `ConnectionError` in every test | No server on `ATENCIOSAMENTE_BASE_URL`. Start it (7.3 step 2) or use the script. |
| `Server exited during startup` | Read the printed log. Usually Postgres isn't reachable, or `POSTGRES_*` is unset in your shell. |
| My dev data is gone | Expected — every test truncates `notifications`. Re-POST what you need. Data management is a later phase. |
| An integration test failed weirdly during a functional run | You ran `ctest` and the suite concurrently. The `TRUNCATE` hit a transaction mid-flight. Run them one at a time. |
| A test hangs ~5s then fails | `ApiClient`'s read timeout. Suspect a pool deadlock; check `build/dev/functional-server.log`. |
| The whole suite hangs forever | Something bypassed `ApiClient` and called `requests` without `timeout=`. There's no CTest timeout to save you here. |
| `Sanitizer report in server log` | A real ASan/UBSan finding in the server under load. The log has the stack. |
| Rows left over after a failure | Expected: `clean_database` resets *before* each test, not after, so a failure leaves evidence. The next run clears it. |
| Venv broken after a container rebuild | `rm -rf tests/functional/.venv`; the next script run rebuilds it. |

---

## 8. CI is deferred — and the Phase 4 collision

### 8.1 What manual-only costs you

Be clear-eyed: **a test tier nobody runs automatically protects nothing.** This suite helps
when you remember to run it before committing. It does not catch a regression that lands
while you're thinking about something else, and it cannot be a merge gate.

That is a real gap, and `PHASE_4_REVIEW_AND_ADJUSTMENTS.md` finding **F4** makes the case
against it in concrete terms: once authorization logic lives in `handlers/`, a bug that
returns another user's notifications breaks **no** unit test and **no** integration test.
A manual tier means that regression ships unless you happened to run the suite that day.

### 8.2 Why it's still the right call for Phase 3 — and how you get CI back

- **The whole tier is new.** Making it a merge gate on day one means a flaky harness
  blocks commits while you're still learning what the harness does.
- **It's the slowest, most environment-dependent tier** — a real server, a real DB, real
  sockets. It has the most ways to be red for reasons that aren't your code.
- **Adopting CI later is a copy-paste, not a redesign**, and the instructions live in the
  code: `scripts/functional.sh` carries a `TODO(ci)` block with the five concrete steps
  (clone the `integration` job, add `python3-venv` to its apt step, swap in
  `./scripts/functional.sh --preset ci -- --junit-xml=...`, upload the XML and server log
  as artifacts, update the workflows README diagram). The script already takes
  `--preset ci`, passes extra args through and exits non-zero on any failure, so nothing
  about it has to change.
- **The `:8080` occupancy check costs nothing on a runner.** A CI job owns the whole
  machine, so the port is free and the check just passes — the shared-database model that
  made a second port pointless locally is also what makes the CI job trivial.

So: build it manual, use it for a few weeks, and promote it when you trust it — not as a
prerequisite for having it at all. **Phase 4 is the natural trigger**, because that's where
§8.1's argument bites.

### 8.3 The collision you must resolve before S1

`PHASE_4_REVIEW_AND_ADJUSTMENTS.md` §S6 plans a **different functional tier**:

| | **This doc** | **`PHASE_4_REVIEW` S6** |
|---|---|---|
| Language | Python / pytest | C++ / Catch2 |
| Dispatch | real socket, separate process | in-process `app.handle_full()` |
| Build impact | none | new `atenciosamente_http` static library, new `tests_functional` target |
| Covers `main.cpp`, HTTP parser, socket | **yes** | no |
| Covers Crow router + handlers + real SQL | yes | yes |
| Flakiness surface | server startup, ports, timeouts | essentially none |
| CI | deferred (TODO in the script) | a third CI job, immediately |
| Isolation | `TRUNCATE` the dev DB | `DELETE ... WHERE email LIKE 'func-%'` |

**Do not build both.** They cover almost the same assertions; the second one to land would
be duplicated maintenance for a small delta in coverage.

How to choose:

- **Want the regression safety net in CI now, and want the exercise to be C++?** Take
  `PHASE_4_REVIEW` S6 instead and treat this document as superseded. The in-process tier
  is faster, can't flake on a port, and is CI-ready — at the cost of never executing
  `main()`, the HTTP parser or a socket, and of a CMake restructure.
- **Want the strongest "tests what the phone talks to" guarantee, cheaply, now?** Take
  this document, and when you reach Phase 4, replace S6 with "extend the Python suite with
  the auth matrix" + act on the `TODO(ci)`.
- **A defensible hybrid:** this suite now (it's cheap and needs no build changes), and in
  Phase 4 add the in-process C++ tier *only* for the auth matrix, where CI gating actually
  matters. Costs the duplication you were trying to avoid, so only do this deliberately.

**Whichever you pick, record the decision and the rejection in `PROJECT_PLAN.md` §10**,
and edit the loser so a future session doesn't implement it by accident. That edit is part
of S1's commit, not a someday task.

---

## 9. Decisions to make (record each in PROJECT_PLAN.md §10)

- **This file's name / where it lands in §6 (before S1):** recommended rename to drop the
  `PHASE_3_` prefix rather than renumber the roadmap again.
- **This tier vs `PHASE_4_REVIEW` S6's in-process tier (before S1):** see §8.3. Record the
  choice *and* the rejection.
- **Python instead of a C++ HTTP client (S1):** the §3.1 argument. The decision most worth
  writing down carefully, since it reverses this repo's default.
- **Libraries (S1):** recommended `pytest` + `requests` + `psycopg[binary]`; note the
  `psql`-subprocess alternative that was declined.
- **Out-of-process vs in-process (S1):** recommended out-of-process.
- **Isolation: per-test `TRUNCATE` of the dev database, no dedicated DB, no name guard
  (S1):** the trade is fresh-start tests now, data management later. Record that dev data
  is destroyed every run, and that a guard must be reinstated before Phase 5 — the `TODO`
  in `conftest.py` is the in-code half of this entry.
- **No `PORT` env var / reuse `8080` (S2):** with one shared database two servers can't
  usefully coexist, so the honest model is one server at a time plus an occupancy check.
  `PORT` is deferred to the deploy phase, where it has a real job.
- **`python3-venv` in `Dockerfile.dev`, venv at `tests/functional/.venv` (S1):** and the
  note that it invalidates the vcpkg image layer on the next rebuild.
- **Pool size `2` for the run (S2):** smaller than S4's thread count so contention is
  guaranteed.
- **Fail on sanitizer reports in the server log (S2):** recommended yes, excluding
  LeakSanitizer.
- **No `pytest.ini`/markers for now (S1):** `-k` covers subsetting at this size.
- **Crow's response for a wrong method on a registered path (S3):** verify `405` vs `404`,
  then record what it actually does.
- **CI deferred, with the wiring recorded as `TODO(ci)` in the script (S2/S5):** plus an
  honest record that §8's CI diagram is knowingly unfulfilled, and that Phase 4 is the
  expected trigger to build it.

---

## 10. Files added / changed (map)

```
backend/
├── Dockerfile.dev                                 S1  (+ python3-venv)
├── scripts/
│   ├── functional.sh                              S2  (new — incl. the TODO(ci) block)
│   └── dev.sh                                     S2  (functional subcommand)
└── tests/
    └── functional/                                     ← Python; NOT a CMake target
        ├── requirements.txt                       S1  (new)
        ├── conftest.py                            S1  (new — fixtures + TODO(data-management))
        ├── test_smoke.py                          S1  (new)
        ├── test_notifications_api.py              S3  (new)
        └── test_concurrency.py                    S4  (new)

.gitignore                                         S1  (.venv/, __pycache__/, .pytest_cache/)
.claude/skills/backend-add-test/SKILL.md           S5
Documentation/PROJECT_PLAN.md                      S1–S4 (§10 rows) / S5 (§8)
Documentation/reference/project_structure.md       S5
CLAUDE.md                                          S5  (quick reference)
Documentation/phase-prompts/PHASE_4_REVIEW_AND_ADJUSTMENTS.md
                                                   S1  (§8.3: mark S6 superseded or
                                                        this doc as the rejected option)
```

**Unchanged on purpose:** everything under `backend/src/`, `backend/vcpkg.json`,
`backend/CMakeLists.txt`, `backend/tests/CMakeLists.txt`, `backend/CMakePresets.json`,
`.env.example`, `.github/workflows/`. There is **no C++ in this phase**. If a step wants to
touch any of these, something has drifted from the plan — stop and re-read §3.2.

No mobile changes this phase.

---

## 11. How to execute

- **One focused conversation per step**, in order, each ending in a commit, the same
  rhythm as the earlier phase docs.
- **Resolve §8.3 first.** If you pick the in-process C++ tier, don't start S1 — this whole
  document is the wrong plan and should be marked superseded instead.
- **Attach `PROJECT_PLAN.md`** (and this file) to every step, and back-port any new
  decision into its §10 decision log.
- **Delegate to the subagent:** the `backend` subagent owns S1–S4 (it's backend tooling
  even where the language is Python). S5 is docs only.
- **Nothing to push for CI's sake.** Every step is fully verifiable locally with
  `scripts/dev.sh functional`, which is the whole point of this shape. Push when you'd
  normally push.
- Prefer to run each step **inside the WSL repo** so `/skill` commands and the subagent
  resolve.

When all steps are green and committed, tick the §1 checklist. The pyramid in
`PROJECT_PLAN.md` §8 then has all three rungs — two of them gated by CI, and one of them
yours to remember to run, with the instructions for closing that gap sitting in
`scripts/functional.sh` for whenever you want them.
