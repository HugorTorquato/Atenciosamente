# Atenciosamente — Action Plan

> What to do next, in what order, and why. Companion to [`PROJECT_ANALYSIS.md`](PROJECT_ANALYSIS.md).
>
> **Date:** 2026-09-18 · **Baseline commit:** `29189b3` · **Method:** four parallel planning
> agents, each critiqued and revised, with every load-bearing claim verified against source.

---

## How to use this document

Every task is a **card** with the same shape, ending in a **`▶ prompt`** block. That block is
the thing you copy — open a fresh conversation, paste it, done. It follows the convention
already established in `PHASE_1_PERSISTENCE.md` S3/S4, including the literal `[GIT LOG HERE]`
placeholder you replace with `git log --oneline -10`.

Every prompt asks the agent to **explain the concept it is exercising**. That is deliberate and
matches the existing phase prompts — this is a learning project, and an unexplained correct
patch is a failed outcome here.

**One rule to adopt before card 1.** `CLAUDE.md` forbids mixing docs and code in one commit,
and also requires back-porting decisions into `PROJECT_PLAN.md` §10. Those collide. The
resolution used throughout: **a §10 row documenting the change ships with that code commit**
(it is that change's own rationale); **corrections to pre-existing stale prose go in a
docs-only commit.** This is an interpretation, not something your house rules state — overturn
it deliberately rather than by accident.

**Categories:** `Must-fix` (something is broken or blocking) · `Should-do` (real value, not
blocking) · `Nice-to-have` (do it when the trigger fires, not before).

---

## Where you actually are

Phase 1 is **functionally complete** — S1–S7 all shipped, all four required decisions logged.
But "done with a phase" here means three things and only one has happened:

1. ✅ The code works.
2. ❌ The repo's description of itself is true.
3. ❌ The tests guarding the phase assert what they claim.

```mermaid
flowchart LR
    P0["Phase 0<br/>walking skeleton"] --> P1["Phase 1<br/>persistence"]
    P1 --> now{"you are here"}
    now --> A["Track A<br/>close out Phase 1"]
    now --> D["Track D<br/>tooling multipliers"]
    A --> B["Track B<br/>Phase 2: concurrency"]
    D --> B
    C["Track C<br/>API hardening"] -.->|"independent"| B
    B --> P3["Phase 3<br/>scheduling"]
    P3 --> P4["Phase 4<br/>push (needs deploy rung 2)"]
```

**Four tracks, 39 cards.** Two of them ship no features at all and should still go first.

| Track | What | Cards | Do it when |
|---|---|:---:|---|
| **A** | Close out Phase 1 — make the repo stop lying, fix the defective test | 10 | Now |
| **D** | AI tooling force multipliers — hooks, skills, the map agents read | 15 | Now, interleaved with A |
| **B** | Phase 2 — measure, introduce shared state, make it safe | 10 | After A1, A5, D1–D5 |
| **C** | API hardening | 4 | Any time; C2 before B2 ideally |

**The two hard Phase 2 prerequisites are A1 (pin dependencies) and A5 (sanitizers in CI).**
Nothing in B6–B9 should start until both have landed.

### Start here — the first three sessions

1. **A1** — pin `vcpkg` (`builtin-baseline`). Three lines. Makes an existing §10 claim true.
2. **D1 + D2** — track `settings.json`, then the `SessionStart` hook. ~1 hour, and it kills the
   manual `[GIT LOG HERE]` paste from every prompt in this document.
3. **A2 + D3** — the documentation truth pass. **These two must land in the same session**, or
   you fix three of four stale phase-state copies and the survivor looks authoritative.

---

## Track A — Close out Phase 1

Order: `A1 → A2 → A3 → A4 → A5 → A6 → A7 → A8 → A9 → A10`. A10 last of the CI cards
deliberately — it is the only one whose first run has unknown fallout.

---

### A1 — Pin the vcpkg dependency set with `builtin-baseline`
**Must-fix** · S · Learning: Medium · Depends on: nothing

An upstream vcpkg commit can break your build on a day you touched no C++, and CI (which clones
vcpkg at HEAD `--depth 1`) resolves a *different* set from your laptop every run. You have been
bitten by this class of change once already — §10's 2026-07-27 row records `exec_params` going
`[[deprecated]]` under `-Werror`.

After this lands, `PROJECT_PLAN.md:268` and `:272` need **no** edit — they claim the pin lives
in `builtin-baseline`, and this makes that true. That is why it is code-only and goes first.

**DoD:** `grep -c builtin-baseline backend/vcpkg.json` returns 1; a clean configure with an
empty binary cache resolves Crow 1.3.3 / libpqxx 8.0.2 / nlohmann-json 3.12.0 / Catch2 3.15.2;
**and the CI integration job passes** — that last one is the real gate, because a shallow vcpkg
clone must still fetch the pinned baseline commit.
**Commit:** `Backend (Build): pin vcpkg builtin-baseline`

```
Use the backend subagent to pin the vcpkg dependency set.
Attached: PROJECT_PLAN.md and ACTION_PLAN.md. Recent history:
[GIT LOG HERE]

Goal: backend/vcpkg.json lists four bare dependency names and has no builtin-baseline,
so my laptop and CI (which clones vcpkg at HEAD --depth 1) resolve package versions
independently. Add a builtin-baseline pinning the exact vcpkg commit that produced the
set I'm on today (Crow 1.3.3, libpqxx 8.0.2, nlohmann-json 3.12.0, Catch2 3.15.2), then
verify a clean configure with an empty binary cache still resolves that same set.
Explain what builtin-baseline actually pins versus what an "overrides" block would pin,
and tell me whether CI's shallow --depth 1 vcpkg clone can still resolve a baseline
commit that isn't HEAD — if it can't, fix the workflow too.
When done: add a §10 row recording the pinned baseline and the reason, and commit in
`Scope (Tag): summary` style (no body, no trailers).
```

---

### A2 — Close out Phase 1 in the documentation
**Must-fix** · M · Learning: Low (engineering value High) · Depends on: A1

Four documents every fresh session reads are wrong the same way: `README.md:18` says "Phase 0 —
Not yet usable"; `CLAUDE.md:37` says Phase 1 is next; `PHASE_1_PERSISTENCE.md:21-32` has eight
unticked boxes for shipped work; `project_structure.md` documents a `backend/Dockerfile` that
does not exist, omits `tests/integration/` and `mobile-ci.yml`, and describes a client with
only `fetchNotifications()`. Three decisions the code made are recorded nowhere.

**Land this in the same session as D3**, which fixes the fourth stale copy.

**DoD:** no file outside `PHASE_0_*` claims the phase is 0 or 1-pending; all eight §1 boxes
ticked; `project_structure.md` has no `Dockerfile` entry and lists `tests/integration/` +
`mobile-ci.yml`; §10 gains three rows.
**Commit:** `Docs (Phase 1): close out persistence phase`

```
Use the backend subagent to close Phase 1 out in the documentation.
Attached: PROJECT_PLAN.md and ACTION_PLAN.md. Recent history:
[GIT LOG HERE]

Goal: Phase 1 shipped at 29189b3 but four docs still describe an earlier repo. Fix, in
one commit: README.md:18 ("Phase 0 — Not yet usable"); CLAUDE.md:37 (says Phase 1 is
next); the eight unticked boxes in PHASE_1_PERSISTENCE.md §1; and
reference/project_structure.md, which documents a backend/Dockerfile that does not
exist, omits tests/integration/ and mobile-ci.yml, and describes an API client with only
fetchNotifications(). Verify every path you write against the working tree — do not copy
the existing tree listing forward.
Then add three §10 rows for decisions the code made that no document records: main.cpp:6
calls .multithreaded() (and because every request opens its own connection there is no
shared mutable state, so the server is thread-safe by construction — say so, it is the
premise of Phase 2); the deliberate choice to let exceptions unwind into Crow's default
handler, argued only in the comment at handlers/notifications.cpp:29-40; and the
{"error": "..."} response envelope, which notifications_client.dart:60-62 parses as a
contract.
Docs only — no code changes in this commit.
When done: commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### A3 — Correct the `exec_params` claim in the repository header
**Should-do** · S · Learning: Low · Depends on: nothing

`notification_repository.hpp:46` says insert "Uses `txn.exec_params(...)`"; `:147` uses
`txn.exec(sql, pqxx::params{...})`. §10's 2026-07-27 row records why it had to change. The
header is the most-read teaching surface in the repo and it currently teaches the API that
breaks the build.

**DoD:** `grep -rn exec_params backend/src/` returns only lines describing it as *deprecated*.
**Commit:** `Backend (Docs): correct the exec_params reference in the repository header`

```
Use the backend subagent to fix a stale comment in the repository header.
Attached: PROJECT_PLAN.md and ACTION_PLAN.md. Recent history:
[GIT LOG HERE]

Goal: notification_repository.hpp:46 says insert() "Uses txn.exec_params(...)", but the
implementation at notification_repository.cpp:147 uses
txn.exec(sql, pqxx::params{txn, title, body}) — see the §10 row dated 2026-07-27 for why
it changed. Rewrite that comment so it describes the code as written, keeps the
SQL-injection-safety explanation intact, and notes that exec_params is the deprecated
spelling of the same guarantee. While you're there, explain what the first element of
pqxx::params{txn, ...} is doing — the .cpp comment says it selects the connection's text
encoding rather than being a bind value, and I want to understand that properly.
Comment only — no behaviour change.
When done: commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### A4 — Split the defective ordering test into two truthful ones
**Must-fix** · S · Learning: **High** · Depends on: nothing

`notification_repository_test.cpp:72-93` is named "get_all() orders rows most-recent-first" but
inserts both rows in one `pqxx::work`. Postgres's `now()` is `transaction_timestamp()`, so both
rows share a `created_at` and the assertion is carried **entirely by the `id DESC` tiebreak**.
It would stay green if someone deleted `created_at DESC` from the query.

Three obvious fixes are all wrong, and knowing why is the lesson: `clock_timestamp()` needs a
migration and still isn't observable (timestamps format to whole seconds); two transactions
breaks rollback isolation; a timestamp parameter on `insert()` widens a production signature
for a test.

**DoD:** two cases where there was one. The `ORDER BY` is inline at
`notification_repository.cpp:100` — deleting `created_at DESC` must fail the new timestamp
test, deleting `, id DESC` must fail the renamed tiebreak test. Run both, then revert.
**Commit:** `Backend (Tests): split the ordering test into tie-break and timestamp cases`

```
Use the backend subagent to fix a defective integration test.
Attached: PROJECT_PLAN.md and ACTION_PLAN.md. Recent history:
[GIT LOG HERE]

Goal: the TEST_CASE at tests/integration/notification_repository_test.cpp:72-93 is named
"get_all() orders rows most-recent-first" but inserts both rows in a single pqxx::work.
Postgres's now() is transaction_timestamp(), fixed for the whole transaction, so both
rows share a created_at and the assertion is satisfied purely by the `id DESC` tiebreak —
it never tests timestamp ordering at all. Split it in two: (1) rename the existing case
to "get_all() breaks created_at ties by id, newest id first", keep the assertion, and
comment why same-transaction inserts always tie; (2) add "get_all() orders rows by
created_at, most recent first" that bypasses insert() and seeds two rows directly in the
test's own transaction with explicit distinct timestamps (created_at is DEFAULT now(),
not GENERATED, so a test may supply it — unlike id), then asserts the newer row comes
first. Stay inside one never-committed transaction: no migration, no production signature
change, no second transaction.
Use the backend-add-test skill. Prove both tests bite: the ORDER BY is inline in the query
string at notification_repository.cpp:100 — temporarily drop one term at a time and show
me which test goes red, then revert. Explain why clock_timestamp() and a timestamp
parameter on insert() are both the wrong fix here.
When done: commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### A5 — Run the test suites under ASan/UBSan in CI, and add warning flags to test targets
**Must-fix (before Phase 2)** · M · Learning: **High** · Depends on: A1

Sanitizers exist only in the `dev` preset and both CI jobs configure `--preset ci` — so ASan
and UBSan have **never run on a commit you didn't personally build**. Phase 2 introduces the
first shared mutable state in the codebase.

> Sanitizer flags **are** already correctly applied to both test targets, compile and link
> (`tests/CMakeLists.txt:17-25`, `:60-68`). Don't re-add them. What's missing from the test
> targets is `-Wall -Wextra -Wpedantic -Werror`, which both production targets have.

**DoD:** a third CI job runs both tiers under ASan+UBSan; a deliberately introduced heap
overflow makes it red and leaves `unit`/`integration` green; both test targets compile with the
warning flags.
**Commit:** `Backend (CI): run unit and integration tests under ASan/UBSan`

```
Use the backend subagent to make sanitizers run in CI.
Attached: PROJECT_PLAN.md and ACTION_PLAN.md. Recent history:
[GIT LOG HERE]

Goal: ENABLE_SANITIZERS is only set by the `dev` preset, and both CI jobs use
`--preset ci`, so ASan/UBSan have never run on a commit I didn't build myself — and
Phase 2 is about to introduce the first shared mutable state in this codebase. Add a
sanitized CI path (a new preset plus a parallel job with no `needs:`, matching the
existing unit/integration split) that runs both test binaries under ASan+UBSan, and add
the -Wall -Wextra -Wpedantic -Werror flags that tests_unit and tests_integration are
missing. Note: the sanitizer flags on the test targets are already there and correct
(tests/CMakeLists.txt:17-25 and :60-68) — read that file before changing anything; only
the warning flags are absent. Prove the job works by temporarily introducing a heap
overflow in a test and showing it go red.
Explain as you go: what ASan and UBSan each instrument, why the sanitizer runtime must be
linked into the executable and not just the static lib, whether a sanitized job should be
Debug or Release, and why TSan needs a separate third preset rather than joining this one.
When done: add a §10 row for the new preset/job and commit in `Scope (Tag): summary`
style (no body, no trailers).
```

---

### A6 — Add the clang-format check job CI has been promising
**Should-do** · S · Learning: Low · Depends on: nothing

`PROJECT_PLAN.md:224` commits to `clang-format --dry-run --Werror` alongside tests and no such
job exists; formatting is enforced only as a side effect of `dev.sh:62` building locally, which
is how you get sweep commits like `f661fad`. The section is already written at
`backend/contexts/github-actions.md:384`.

**DoD:** a `lint` job with no `needs:` reports as its own check; mangling one file's indentation
makes it red and leaves the others green.
**Commit:** `Backend (CI): add a clang-format check job`

```
Use the backend subagent to add the lint job CI already promises.
Attached: PROJECT_PLAN.md and ACTION_PLAN.md. Recent history:
[GIT LOG HERE]

Goal: PROJECT_PLAN.md:224 promises "clang-format --dry-run --Werror alongside tests" and
no such job exists — formatting is currently enforced only as a side effect of
scripts/dev.sh:62 running clang-format -i locally. Add a `lint` job to backend-ci.yml as
a parallel job (no `needs:`, matching the existing unit/integration split so it reports as
its own status check). backend/contexts/github-actions.md:384 already has this section
written — the find/xargs pipeline there is correct on extension matching, but tighten two
things: it has no -print0/-0 so a path containing a space would break it, and with no
matches xargs would invoke clang-format with zero arguments. Keep the path filters
consistent with the other jobs.
Explain why --dry-run --Werror is the right shape for a gate versus having CI rewrite
files, and whether this job should also cover backend/tests.
When done: commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### A7 — Replace the placeholder widget test and run `flutter test` in CI
**Should-do** · S · Learning: Medium · Depends on: nothing

`test/widget_test.dart` asserts `1 + 1 == 2`, and `mobile-ci.yml` has no `flutter test` step —
zero coverage *plus* a file that reads like coverage. A false signal is worse than an honest gap.

**DoD:** `flutter test` runs between `analyze` and `build apk` and fails the job when a test
fails; no test asserts a tautology.
**Commit:** `Mobile (Tests): replace the placeholder test and run flutter test in CI`

```
Use the frontend subagent to give the app one real test and make CI run it.
Attached: PROJECT_PLAN.md and ACTION_PLAN.md. Recent history:
[GIT LOG HERE]

Goal: test/widget_test.dart asserts 1 + 1 == 2, and mobile-ci.yml never runs
`flutter test` — so the app has no coverage and a file that reads like coverage. Replace
the placeholder with one or two genuine widget tests on NotificationsScreen (it renders
the notifications it is given; it shows an error state when the fetch fails), and add a
`flutter test` step to mobile-ci.yml between analyze and build apk. Keep it minimal and
dependency-light — I do not want a mocking framework in this app if a plain injected
function or a fake client class will do; if you think one is justified, ask first.
Coming from C++, explain what pumpWidget and WidgetTester actually do, why a widget test
needs `pump` at all, and how this differs from the backend's unit tests.
When done: commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### A8 — Validate database config at startup instead of per request
**Should-do** · S · Learning: Medium · Depends on: nothing

`connection.cpp:16-27` validates the `POSTGRES_*` vars inside `make_connection()`, which runs
per request — a typo'd var produces a 500 on request #1 rather than a refusal to boot, and the
process reports healthy throughout. Contradicts §4.1. Same card fixes `main()` having no
try/catch (port-in-use gives `std::terminate`, not a message). Not wasted work: Phase 2's pool
must be constructed at boot, which forces eager config anyway.

**DoD:** unset `POSTGRES_PASSWORD` exits non-zero before binding the port, naming the variable;
starting twice on 8080 prints a message instead of aborting; `docker compose up` still clean.
**Commit:** `Backend (Feat): validate database config at startup`

```
Use the backend subagent to make the server fail fast on bad configuration.
Attached: PROJECT_PLAN.md and ACTION_PLAN.md. Recent history:
[GIT LOG HERE]

Goal: db/connection.cpp:16-27 validates the POSTGRES_* env vars lazily, inside
make_connection(), which runs per request — so a typo'd variable surfaces as a 500 on the
first request rather than a refusal to boot, contradicting PROJECT_PLAN §4.1. Validate
configuration once at startup and exit non-zero with a clear message if it's wrong. Also
wrap main.cpp's body in a try/catch: right now a port-in-use gives std::terminate instead
of a message.
Then argue the trade-off with me before committing to it: should startup also open one
probe connection to Postgres (catching a wrong password, not just a missing variable),
given that would make the server refuse to start when the DB is briefly down? Consider
what docker-compose's depends_on: service_healthy already guarantees. Keep the change
small — Phase 2's connection pool will be constructed at boot and will absorb this seam.
When done: record the decision in PROJECT_PLAN.md §10 and commit in
`Scope (Tag): summary` style (no body, no trailers).
```

---

### A9 — Fix `migrate.sh`'s hand-built `DATABASE_URL` and the comment that lies about it
**Should-do** · S · Learning: Low-Medium · Depends on: nothing

Two defects. `migrate.sh:20` interpolates the password into a `postgresql://` URL with no
percent-encoding, so `@`, `:`, `/` or `#` silently produces a wrong connection string — notable
because `connection.cpp:35-45` takes exactly this care on the C++ side. And the comment at
`:6-8` claims "CI sets this directly" — **nothing sets it**; `grep -rn DATABASE_URL .github/`
returns nothing and the integration job passes `POSTGRES_*` only.

> **Do not** add an advisory lock or migration checksums. You are one developer with one CI job
> and a `schema_migrations` primary key that already makes a double-apply a no-op. **Trigger
> for either: an environment you cannot rebuild from scratch.**

**DoD:** no `postgresql://` constructed; a password containing `@` and `/` applies migrations;
the comment describes what actually happens.
**Commit:** `Backend (Fix): use libpq env vars in migrate.sh instead of a hand-built URL`

```
Use the backend subagent to fix the migration runner's connection handling.
Attached: PROJECT_PLAN.md and ACTION_PLAN.md. Recent history:
[GIT LOG HERE]

Goal: two defects in scripts/migrate.sh. (1) Line 20 builds
postgresql://${POSTGRES_USER}:${POSTGRES_PASSWORD}@... with no URL-encoding, so a password
containing @ : / or # produces a silently wrong connection string — notable because
db/connection.cpp:35-45 takes exactly this care on the C++ side. Rather than adding a
percent-encoder, drop the composed URL and let psql read
PGHOST/PGPORT/PGUSER/PGPASSWORD/PGDATABASE, the same libpq mechanism the C++ side uses.
(2) The comment at :6-8 claims "CI sets $DATABASE_URL directly, per
contexts/github-actions.md's Phase 1 job" — that is false. Nothing in .github/ sets
DATABASE_URL; the integration job passes POSTGRES_* only and lets line 20 compose the URL.
Don't go looking for it. Keep the DATABASE_URL override as a manual convenience (useful
for pointing the script at a scratch database), but rewrite the comment to say that's
what it is.
Also quote the `\i $file` at :40. Test by putting a password with @ and / in .env.
Explain libpq's env-var precedence rules and how they relate to the conninfo string
make_connection() builds.
Do NOT add an advisory lock or migration checksums — deliberately deferred until there's
an environment I can't rebuild from scratch. Leave contexts/github-actions.md:309 (which
repeats the same false claim) alone — it's a doc and belongs in a separate commit.
When done: commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### A10 — Wire `.clang-tidy` into the build behind a CMake option
**Should-do** · M · Learning: **High** · Depends on: A5 · **Last of the CI cards**

`.clang-tidy` is a carefully written config — `bugprone-*`, `clang-analyzer-*`,
`WarningsAsErrors: "*"` — that **nothing invokes**. As a gate it's nice-to-have; as a *teacher*
of modern C++ it's the highest-value item in this track, because it finds real things in your
own code and explains them. It goes last because it is the only card whose first run has
unknown fallout.

**DoD:** the tidy preset builds clean with no suppressions added purely to reach zero; CI fails
on a deliberately introduced `bugprone-*` violation; any disabled check carries a one-line reason.
**Commit:** `Backend (Build): wire clang-tidy behind ENABLE_CLANG_TIDY`

```
Use the backend subagent to make .clang-tidy actually run.
Attached: PROJECT_PLAN.md and ACTION_PLAN.md. Recent history:
[GIT LOG HERE]

Goal: backend/.clang-tidy is a real config — bugprone-*, clang-analyzer-*, a
HeaderFilterRegex scoped to src/, WarningsAsErrors "*" — and nothing invokes it: no
CMAKE_CXX_CLANG_TIDY anywhere and no CI step. Wire it up behind an ENABLE_CLANG_TIDY
option (mirroring ENABLE_SANITIZERS), add a preset that turns it on, and add a CI job.
Then run it and walk me through every finding before fixing anything: for each one, tell
me what the check protects against and whether the code is actually wrong or the check is
a poor fit here. Fix the real ones; if you disable a check, put a one-line reason in
.clang-tidy. Do not add NOLINT suppressions just to reach zero. Watch out for tidy trying
to lint Crow and vcpkg headers — tell me whether HeaderFilterRegex alone is enough or
whether compile_commands.json needs filtering. If the fallout is large, land the wiring
plus fixes first with the option defaulting OFF, and flip CI on in a second commit.
When done: record the decision in PROJECT_PLAN.md §10 and commit in
`Scope (Tag): summary` style (no body, no trailers).
```

---

## Track D — Tooling force multipliers

These ship **no features**. They make every card in A, B and C cheaper. D1–D5 are ~2 hours
total; do them before any Phase 2 code.

**Order by payoff:** `D1 → D2 → D3 → D4 → D5 → D6 → D7 → D8 → D15 → D9 → D10 → D11 → D12 → D13 → D14`

> `[GIT LOG HERE]` stays literal in D1 and D2's prompts. From D2 onward the hook supplies it —
> that omission in later prompts *is* D2's payoff.

---

### D1 — Track `.claude/settings.json` with the real allowlist and one trivial hook
**Must-fix** · S · Depends on: nothing

The only settings file is untracked `settings.local.json` with **two** allow entries, for a
project whose entire inner loop is `cmake`/`ctest`/`git log`. This card also lands the `hooks`
key with a deliberately trivial hook, so D2/D8/D9 edit a file whose plumbing is already proven.

> **Permission syntax is the risk here.** Your existing entry uses the space form
> (`Bash(docker compose *)`). The documented prefix form is `Bash(cmd:*)`. They may not be
> equivalent, and if the form is wrong **the whole allowlist silently does nothing** — the only
> symptom is prompts you expected to be gone, which reads as "the hook didn't work" and sends
> you debugging the wrong file. Make `/permissions` confirming the entries are active a hard
> line in the DoD, and consider matching the existing space form for safety.

Allowlist to cover: `cmake`, `ctest`, `docker compose`, read-only git (`log`/`status`/`diff`/
`show`/`ls-files`), `flutter analyze`, `flutter test`, `backend/scripts/dev.sh`. Also add
`.claude/settings.local.json` to `.gitignore` — which currently doesn't mention `.claude/` at all.

**DoD:** `git ls-files .claude/settings.json` prints the path; a fresh session prints the echo
marker; `/permissions` lists the new entries as active; `ctest --preset=dev` prompts for nothing.
**Commit:** `Tooling (AI): track .claude/settings.json with build allowlist and hook plumbing`

```
Main-thread task — no subagent. Attached: PROJECT_PLAN.md.
Recent history:
[GIT LOG HERE]

Goal: create a TRACKED .claude/settings.json holding (a) an expanded Bash permission
allowlist covering this repo's actual loop — cmake, ctest, docker compose, read-only git
(log/status/diff/show/ls-files), flutter analyze/test, backend/scripts/dev.sh — and (b) a
single deliberately trivial SessionStart hook that only echoes a marker. The trivial hook
is a rollout step, not a placeholder: it proves the hooks plumbing works before any hook
that could block a call is added.

IMPORTANT on syntax: the one existing entry in settings.local.json uses the space form,
`Bash(docker compose *)`. The documented prefix form is `Bash(cmd:*)`. Tell me which form
this version actually honours before writing eleven entries in the wrong one — if the
form is wrong the allowlist silently does nothing and the only symptom is prompts I
expected to be gone. Run /permissions afterwards and confirm the entries are listed as
active; do not assume.

Also add .claude/settings.local.json to .gitignore — it is untracked but not ignored.
Make the echo hook print $CLAUDE_PROJECT_DIR so we also learn whether that variable is
exported, since later hooks depend on it.
When done: commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### D2 — Replace the echo hook with a real `SessionStart` context injector
**Must-fix** · S · Learning: **High** · Depends on: D1

`PHASE_0_PROMPTS.md:16-18` tells you to hand-run `git log --oneline -10` and paste it — a human
doing a hook's job, seven times in `PHASE_1_PERSISTENCE.md` alone. The paste instruction also
gives the **wrong repo path** (`~/projetos/atenciosamente`; it's `~/projects/Atenciosamente`).
A hook cannot get the path wrong.

Key implementation details: invoke as `bash <path>`, **not** via a shebang — this repo has
`core.fileMode=false` and lost the executable bit once already (commit `de3e158`). Print only
the Date and Decision columns of §10's tail; the rationale cells run to ~1,500 characters. End
with an unconditional `exit 0` — a context hook must never prevent a session starting.

**DoD:** the script prints commits + §10 tail and exits 0 standalone; a fresh session shows it
with no paste; `grep -rc 'GIT LOG HERE' Documentation/` returns 0.
**Commit:** `Tooling (AI): inject git log and decision-log tail via a SessionStart hook`

```
Main-thread task — no subagent. Attached: PROJECT_PLAN.md.
Recent history:
[GIT LOG HERE]

Goal: replace the trivial SessionStart echo hook with .claude/hooks/session-context.sh,
printing (1) git log --oneline -10, (2) git status --short, and (3) the Date + Decision
columns of the last 5 rows of PROJECT_PLAN.md section 10. Wire it as
`bash .claude/hooks/session-context.sh` — through bash rather than a shebang, because
core.fileMode=false in this repo already cost us commit de3e158 restoring executable bits.

Constraints: the script must end in `exit 0` on every path; must NOT print the rationale
column (those cells are ~1500 chars each); and must not use `cut -c` to truncate (it
byte-splits the em-dashes those rows are full of).

Then sweep the [GIT LOG HERE] placeholder and its paste instructions out of
Documentation/phase-prompts/PHASE_0_PROMPTS.md and PHASE_1_PERSISTENCE.md — the hook
supplies it now. Note that PHASE_0_PROMPTS.md:17 gives the wrong repo path
(~/projetos/atenciosamente); the hook makes that line moot.

Test standalone first: `bash .claude/hooks/session-context.sh; echo "exit=$?"`. Show me
the output before wiring it up.
When done: commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### D3 — Delete the three stale phase-state blocks; point everything at §10
**Must-fix** · S · Depends on: D2 · **Land with A2**

The same fact lives in four places and three are wrong. The fix is not to update four copies —
updating four copies is what produced this. Delete three, let §10's tail (now auto-injected) be
the single answer.

**What is lost:** a human skimming `CLAUDE.md` no longer gets an at-a-glance "where are we".
Worth giving up — that line has been false for two months, and a wrong answer is worse than a
pointer. **Tick, don't delete**, the `PHASE_1_PERSISTENCE.md` boxes: that file is the record of
what Phase 1 *was*.

**DoD:** `grep -rn "Phase 0 (walking skeleton) is complete\|Remaining: \*\*S7\*\*" CLAUDE.md .claude/`
returns nothing; all eight boxes ticked; `CLAUDE.md` names §10 as the sole source.
**Commit:** `Docs (State): delete duplicated phase status, point at PROJECT_PLAN section 10`

```
Main-thread task — no subagent. Attached: PROJECT_PLAN.md.

Goal: phase status is duplicated in four places and three are stale. Delete the three,
keep PROJECT_PLAN.md section 10 as the only one.

1. CLAUDE.md:37-40 — "Phase 0 (walking skeleton) is complete. Next is Phase 1" is wrong;
   Phase 1 finished at 29189b3. Replace the whole block with a single line saying the
   current phase is the tail of PROJECT_PLAN.md section 10, injected automatically by the
   SessionStart hook, and that no other file states the phase.
2. .claude/agents/backend.md:65-70 — claims S7 is remaining. S7 IS 29189b3. Delete the
   section outright (do not update it — updating it is what got us here).
3. Documentation/phase-prompts/PHASE_1_PERSISTENCE.md:21-32 — tick all eight checkboxes;
   the work is done and traceable b1fa9bc..29189b3. Tick, do not delete: that file is the
   record of what Phase 1 was.

Do not touch any other content in those files.
When done: commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### D4 — Refresh `project_structure.md`, the map every backend session orients from
**Must-fix** · S · Depends on: D1

`agents/backend.md:20` sends every backend session here *first*, and it is wrong four ways.
It also mentions `.claude/` exactly once, in passing, at `:222` — no section describes the AI
setup that half this plan modifies.

> The file is **not** uniformly rotten — the `src/` tree at `:52-72` is accurate down to the
> four-layer split. Fix the four wrong things; resist rewriting what's right.

**DoD:** no `Dockerfile` line without `.dev`; `tests/integration/` and `mobile-ci.yml` present;
a `## AI setup — .claude/` section exists.
**Commit:** `Docs (Reference): refresh project_structure for integration tests, mobile CI, and .claude`

```
Main-thread task — no subagent. Attached: PROJECT_PLAN.md and
Documentation/reference/project_structure.md.

Goal: this file is what .claude/agents/backend.md:20 tells every backend session to
orient from, and four things in it are false. Fix exactly those:

1. :36 lists `backend/Dockerfile` (multi-stage prod build). It does not exist — only
   Dockerfile.dev does. Delete the line.
2. The tests/ block at :74-80 shows only `unit/`. Add `integration/` with
   notification_repository_test.cpp, the tests_integration target, and the per-test
   transaction-rollback isolation (landed 146518d).
3. :19-21 lists only backend-ci.yml under .github/workflows/. Add mobile-ci.yml
   (pub get -> flutter analyze -> flutter build apk --debug) and README.md.
4. There is no section for .claude/ — it is mentioned once in passing at :222. Add a
   `## AI setup — .claude/` section: agents/{backend,frontend}.md, the six skills, the
   tracked settings.json, and hooks/.

Verify each claim against the filesystem before writing it. Do not rewrite the src/ tree
at :52-72 — I checked it and it is correct.
When done: commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### D5 — Fix the two backend skills that teach rejected mechanisms and phantom paths
**Must-fix** · S · Depends on: nothing

`backend-add-migration:48-53` tells the agent to apply migrations with `psql < file` —
`PROJECT_PLAN.md:294` records the opposite decision, choosing `scripts/migrate.sh` explicitly
because it is *"trackable and repeatable unlike manual `psql <`"*. `backend-add-endpoint:29`
teaches `crow::json::load`, replaced per `PROJECT_PLAN.md:314`.

**And both skills point at `backend/include/atenciosamente/…`, a directory that has never
existed in this repo's history** (`git log --all -- backend/include` is empty). Exact
locations: `backend-add-endpoint:28` and `backend-add-migration:29`.

Also `backend-add-endpoint:33` says to check whether "the glob" picks up a new `.cpp`. There is
no glob — `CMakeLists.txt` uses explicit file lists, and handlers go in the **executable's**
list, which is precisely why they have no tests.

**DoD:** `grep -rn "psql.*<\|crow::json::load\|include/atenciosamente" .claude/skills/` returns
nothing; the migration skill's apply step is `backend/scripts/dev.sh migrate`; the endpoint
skill's step 6 delegates to `backend-add-test`.
**Commit:** `Tooling (AI): correct migration and endpoint skills to the mechanisms actually in use`

```
Main-thread task — no subagent. Attached: PROJECT_PLAN.md.

Goal: two backend skills teach mechanisms this repo explicitly rejected, plus a path that
has never existed. Fix them.

.claude/skills/backend-add-migration/SKILL.md
 - :14-18 drop the "if no migration setup exists yet, confirm the convention" preamble —
   the convention shipped at b1fa9bc.
 - :29 points at include/atenciosamente/notification.hpp. That directory has NEVER
   existed — `git log --all -- backend/include` is empty. The headers are
   backend/src/domain/notification.hpp and notification_json.hpp.
 - :48-53 replace the `docker compose exec -T db psql ... < file` block with
   `backend/scripts/dev.sh migrate` (wraps scripts/migrate.sh: idempotent, tracks applied
   files in schema_migrations, runs automatically before dev.sh run and dev.sh test).
   PROJECT_PLAN.md:294 records exactly this decision, with "trackable and repeatable
   unlike manual psql <" as the reason.

.claude/skills/backend-add-endpoint/SKILL.md
 - :28 same phantom include/atenciosamente path.
 - :29 teaches crow::json::load(req.body). PROJECT_PLAN.md:314 replaced it with
   nlohmann::json::parse(req.body, nullptr, false) — one JSON library both directions, and
   validation stays unit-testable without linking Crow into atenciosamente_core.
 - :33 says to check whether "the glob" picks up a new .cpp. There is no glob:
   backend/CMakeLists.txt uses explicit source lists, and handlers/ plus app.cpp are in
   the SERVER EXECUTABLE's list, not atenciosamente_core — which is precisely why they
   have no tests. Say that.
 - :36-39 step 6 defers integration tests to "Phase 1", which is finished. Replace with:
   invoke the backend-add-test skill, which owns tier selection, file placement and CMake
   wiring.

When done: commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### D6 — Rewrite the skill `description:` fields so they actually fire
**Should-do** · S · Depends on: D5

A skill that never triggers is worth zero regardless of body quality, and there is **proof in
your history**: `frontend-add-model`'s description says "when the app needs to consume a new
backend resource" — an inference, not a matchable condition. `29189b3` added a `toJson()` to an
existing model, consuming no new resource, and `PHASE_1_PERSISTENCE.md:316` had to name the
skill by hand because the description wouldn't carry it.

The rule, worth stating once in `CLAUDE.md`: **a description names files and verbs, not
purposes.** You type "add a `toJson()`", not "consume a new backend resource".

**DoD:** every description names paths or concrete triggers; `backend-add-endpoint` no longer
advertises writing a test; "add a `toJson()` to the notification model" fires
`frontend-add-model` without being named.
**Commit:** `Tooling (AI): rewrite skill descriptions around matchable conditions`

```
Main-thread task — no subagent.

Goal: skill descriptions state intent instead of matchable conditions, and one
demonstrably fails to fire: 29189b3 added a toJson() to an existing model, and
PHASE_1_PERSISTENCE.md:316 had to name frontend-add-model explicitly in the prompt
because the description did not carry it.

Rewrite the `description:` frontmatter of all six skills so each names file paths and
concrete verbs:
 - frontend-add-model: "Use when adding or changing a Dart class in lib/models/ that
   mirrors a backend JSON shape — a new fromJson, a toJson for a POST body, or a field
   added to match the C++ struct."
 - backend-add-endpoint: drop "and a Catch2 test" — the body now delegates testing to
   backend-add-test, so the description must stop competing for "add an endpoint with a
   test".
 - backend-add-test: keep it (best-written one) but make it clearly own the word "test".
 - the other three: same treatment, files and verbs not purposes.

Do not touch skill bodies. Then add one line to CLAUDE.md's skills section stating the
rule: a description names files and verbs, not purposes.
When done: commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### D7 — Extract a `finish-step` skill and delete the copy-pasted commit ritual
**Should-do** · M · Learning: **High** · Depends on: D3, D5, D6

The close-out ritual is duplicated across six skills, each a slightly different restatement, and
only `backend-add-migration` remembers the §10 back-port that `CLAUDE.md:35` requires of
everyone. This is the one procedure recurring in *every* task and the one visibly decaying —
D3 exists because it was never automated.

The §10 ordering rule (`PROJECT_PLAN.md:250`, "new entries go at the bottom") is **already
violated twice**, and it matters now: D2's hook reads `tail -5` as authoritative phase state, so
a misordered row silently corrupts what every session believes. Fix both rows in this commit —
they are the demonstration case.

**DoD:** `grep -c "no body, no trailers" .claude/skills/*/SKILL.md` shows the phrase in
`finish-step` only; `CLAUDE.md` lists all seven skills.
**Commit:** `Tooling (AI): extract the commit-and-log ritual into a finish-step skill`

```
Main-thread task — no subagent. Attached: PROJECT_PLAN.md.

Goal: the commit-and-log ritual is copy-pasted into all six skills, each slightly
different, and only one remembers the PROJECT_PLAN section 10 back-port that CLAUDE.md:35
requires of every task. Extract it.

Create .claude/skills/finish-step/SKILL.md covering:
 1. Append the section 10 row at the TRUE bottom. This rule (PROJECT_PLAN.md:250) is
    already broken twice — the 2026-07-26 migrate.sh row sits at :294 among 2026-05-05
    rows, and the 2026-07-28 CI-split row sits at :318 after two 2026-07-30 rows. It
    matters now because the SessionStart hook reads the last 5 rows to report the current
    phase, so a misordered row corrupts what every session believes.
 2. The row records WHY, not what. Section 10's value is the rationale column.
 3. One-line commit, `Scope (Tag): summary`, no body, no trailers.
 4. Check `git status` before committing — CLAUDE.md:52 forbids sweeping docs and code
    into one commit.

Then replace the commit step in each of the six skills with a one-line pointer to
finish-step, and add finish-step to the CLAUDE.md skill list (which is also currently
missing backend-add-test — fix that too).

Finally, move those two misordered section 10 rows to the bottom in date order. That is
the demonstration case, so do it in this commit.
When done: commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### D8 — Add a **non-fatal** `PostToolUse` clang-format hook
**Should-do** · S · Depends on: D1

`f661fad` reformatted seven files in one sweep because formatting had drifted — exactly the
commit a per-edit hook prevents. Today's mechanism (`dev.sh:62`) runs over the whole tree and
only when you build.

> **The design constraint dominates: `clang-format` exists only inside the dev container, so a
> hook that blocks edits whenever Docker is down will be disabled within a day.** It must exit 0
> on every failure path — never exit 2. Use `python3`; `jq` is not installed on this host.

**DoD:** with the container up, an edit leaves the file formatted; with `docker compose down`
first, the same edit succeeds, prints a skip note, and leaves the file unchanged.
**Commit:** `Tooling (AI): format edited backend C++ files via a non-blocking PostToolUse hook`

```
Main-thread task — no subagent.

Goal: add .claude/hooks/format-cpp.py as a PostToolUse hook on Edit|Write that runs
clang-format on the single backend C++ file just edited, replacing the whole-tree sweep at
backend/scripts/dev.sh:62 that produced commit f661fad.

Hard constraints:
 - The hook payload arrives on STDIN as JSON, not env vars. The edited path is
   tool_input.file_path. Use python3 — jq is NOT installed on this host.
 - clang-format exists only inside the dev container, so shell out via
   `docker compose exec -T backend clang-format -i <path>`. docker-compose.yml mounts
   ./backend at /workspace, so strip the leading "backend/" and prefix "/workspace/".
 - It must exit 0 on EVERY path: docker missing, container down, unparseable JSON, file
   outside the repo. Never exit 2. A formatter that blocks edits when Docker is down gets
   deleted within a day, and then nothing formats anything.
 - Only act on .cpp/.hpp under backend/.

Test standalone BEFORE adding it to settings.json, in this order:
 1. a README.md path -> silent, exit 0
 2. `docker compose down`, then an app.cpp path -> skip note, exit 0, file unchanged
    (check md5sum before and after)
 3. `docker compose up -d backend`, then an app.cpp path -> actually formats
Show me all three results. Only then wire it into .claude/settings.json.
When done: commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### D15 — Build the hook regression harness (do this **before** D9)
**Must-fix** · S · Learning: **High** · Depends on: nothing

D9's gate runs on **every** Bash call. One false positive and no commit in this repo succeeds
through Claude, with a confusing failure (a blocked tool call complaining about commit style on
a command that wasn't a commit). The harness is twenty lines and it is what makes D9 safe.
Building it as its own card is the point — the rollout order is a deliverable.

Must **exit 0**: valid house-style messages, `--amend --no-edit`, `-F file`, bare `git commit`,
`git log --oneline -10`, `git -C /somewhere log --grep commit`, `echo "git commit -m nonsense"`,
`cmake --build ... && ctest ...`, `git add -A && git commit -m "Mobile (Fix): ..."`.
Must **exit 2**: `"add stuff"`, `"feat: add stuff"`, `"Backend: add stuff"` (no tag), two `-m`
flags, an embedded `Co-Authored-By`, `-am "wip"`.

**DoD:** the harness prints PASS for every case and exits non-zero if any disagrees.
**Commit:** `Tooling (AI): add a regression harness for the commit-message and format hooks`

```
Main-thread task — no subagent.

Goal: write .claude/hooks/test-hooks.sh, a regression harness for the hooks, BEFORE any
blocking hook is wired into settings.json. A PreToolUse gate on Bash with one false
positive stops every commit in this repo, and the failure looks confusing — a blocked tool
call complaining about commit style on a command that was not a commit.

It feeds a command string to a hook as stdin JSON ({"tool_input":{"command":...}}) and
asserts the exit code. Build the JSON with python3, not jq.

Must exit 0 (allowed):
  git commit -m "Tooling (AI): add finish-step skill"
  git commit -m "Backend (Feat): add POST /notifications"
  git commit --amend --no-edit
  git commit -F /tmp/msg
  git commit
  git log --oneline -10
  git -C /somewhere log --grep commit
  echo "git commit -m nonsense"
  cmake --build --preset=dev && ctest --preset=dev
  git add -A && git commit -m "Mobile (Fix): correct base URL fallback"

Must exit 2 (blocked):
  git commit -m "add stuff"
  git commit -m "feat: add stuff"
  git commit -m "Backend: add stuff"
  git commit -m "Backend (Feat): x" -m "body here"
  a message containing a Co-Authored-By trailer
  git commit -am "wip"

Print PASS/FAIL per case and exit non-zero if any case disagrees. Add a comment at the top
stating the rule: no blocking hook goes into settings.json until this script is green.
When done: commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### D9 — Add a fail-open `PreToolUse` commit-message gate
**Should-do** · M · Learning: **High** · Depends on: D1, **D15**

The convention holds today by model compliance alone — and it does hold: chaotic before
`d7e349f`, uniform after, zero bodies and zero trailers across 35 commits. Make it structural
before it stops holding.

> **Design rule: FAIL OPEN.** Anything the script cannot confidently parse is allowed through.
> The escape hatch if it ever misfires: commit from your own terminal. This is a Claude-side
> gate, not a git hook — deliberately, because a `commit-msg` git hook would block you too.

**DoD:** every D15 case passes *before* the hook appears in `settings.json`; a real house-style
commit succeeds through the Bash tool.
**Commit:** `Tooling (AI): gate commit messages against the house convention with a PreToolUse hook`

```
Main-thread task — no subagent.

Goal: add .claude/hooks/check-commit-msg.py as a PreToolUse hook on Bash that blocks
commit messages violating CLAUDE.md's convention: `Scope (Tag): summary`, one line, no
body, no trailers, Scope in {Backend, Mobile, Docs, Tooling}.

Hard constraints:
 - Payload on STDIN as JSON; the command is tool_input.command. Use python3, not jq.
   Exit 2 blocks and returns stderr to Claude; exit 0 allows.
 - FAIL OPEN. Unparseable command, unbalanced quotes, no -m flag, `git commit -F file`,
   `--amend --no-edit` — all must exit 0. A false positive blocks every commit in the
   repo, so the acceptable false-positive rate is zero.
 - Handle -m, --message, --message=X, -mX, and combined shorts like -am / -amX.
 - Block: multiple -m flags, embedded newlines, Co-Authored-By, "Generated with", and
   anything failing the regex.
 - Only inspect `git commit` — skip `git -C dir log`, `echo "git commit ..."`. Resolve the
   subcommand properly, honouring -C/-c which take a value.

Do NOT add it to settings.json yet. First run .claude/hooks/test-hooks.sh against it and
show me every case passing. Document in the docstring that -F and editor commits pass
ungated (deliberate), and that the escape hatch if it misfires is to commit from a normal
terminal.
When done: commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### D10 — Delete `organize-docs`; keep its six routing lines in `CLAUDE.md`
**Should-do** · S · Depends on: D7

It codifies a reorganization that ran once at `7ae1f49` — **a day before the skill existed**
(`d7e349f`). It has therefore never driven work. It is failing its own spec right now
(`OPEN_PROJECT_IN_WSL.md` sits unindexed at the docs root, which its step 1 exists to catch).
And its "canonical layout" block is a **third** stale copy of the doc tree. A drift-prevention
skill that is itself a drift surface.

**What survives:** the routing table — "how a tool works" → `concepts/`, "step by step" →
`setup/`, "what exists" → `reference/`, phase plans → `phase-prompts/`, `PROJECT_PLAN.md` stays
at the root. Six lines in `CLAUDE.md`, no maintenance surface.

**Commit:** `Tooling (AI): retire organize-docs, keep its routing rules in CLAUDE.md`

```
Main-thread task — no subagent.

Goal: retire the organize-docs skill.

The case: committed at d7e349f (2026-06-23), one day AFTER the reorg it describes ran at
7ae1f49 (2026-06-22) — so it has never driven any work. It is failing its own spec right
now: Documentation/OPEN_PROJECT_IN_WSL.md sits at the docs root, unindexed, which its step
1 exists to catch. And its canonical-layout block is a third stale copy of the doc tree.

Do this:
 1. Move its routing table (concepts/setup/reference/phase-prompts, and "PROJECT_PLAN.md
    stays at the root") into the CLAUDE.md docs section — six lines, no ASCII tree.
 2. `git rm -r .claude/skills/organize-docs/`.
 3. Fix what it was failing to catch: decide whether OPEN_PROJECT_IN_WSL.md belongs in
    setup/ (I think yes) and index it in Documentation/README.md. Also index the untracked
    Documentation/setup/test_on_device.md.
 4. Remove the organize-docs reference at project_structure.md:222.

Say explicitly in your summary what capability we lost by deleting it.
When done: commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### D11 — Rescue `backend/contexts/`: delete two, promote three, wire them in
**Should-do** · M · Depends on: D10

~900 lines of good reference are invisible — referenced by no agent, no skill, and not by
`CLAUDE.md`. `github-actions.md` (435 lines, current) is the **best CI reference in the repo**
and already contains the ready-written lint job A6 needs. Meanwhile `docker.md` there is
**byte-identical** to `Documentation/concepts/docker.md` (md5 `b2f9c794…`, verified both), and
`ci-setup.md` embeds a full copy of a workflow rewritten twice since.

> Carry `ci-setup.md`'s "how to pin actions to SHA" recipe into `github-actions.md` before
> deleting — unpinned actions are still a live gap. And watch the collision: a
> `Documentation/concepts/github_actions.md` (underscore) already exists.

**Commit:** `Docs (Reorg): promote backend/contexts reference into Documentation/concepts`

```
Main-thread task — no subagent. Attached: PROJECT_PLAN.md.

Goal: backend/contexts/ holds ~900 lines that no agent, skill, or CLAUDE.md line ever
references. Two of six files are dead weight, three are valuable.

Delete:
 - backend/contexts/docker.md — byte-identical to Documentation/concepts/docker.md
   (md5 b2f9c7948d22425ddf0a814d33b02f38 for both, verified).
 - backend/contexts/ci-setup.md — it embeds a full copy of a workflow rewritten twice
   since. BUT first carry its "how to pin actions to SHA" recipe into github-actions.md;
   unpinned actions are still a live gap.

Promote into Documentation/concepts/ with `git mv` (preserve history): github-actions.md
(435 lines, current, and it contains a ready-written lint job we have a card for),
catch2-guide.md, and unit-testing-infrastructure.md.
Careful: Documentation/concepts/github_actions.md already exists with an underscore.
Decide deliberately — merge or rename — do not leave two CI docs side by side.

Decide what to do with cmake.md and say why.

Then wire them in: index them in Documentation/README.md, name them in the "Orient first"
section of .claude/agents/backend.md, and update the backend/contexts/ line in
project_structure.md. PROJECT_PLAN.md:279 is the row that established backend/contexts/ —
add a superseding row rather than editing it.
Also fix contexts/github-actions.md:309, which repeats the false claim that CI sets
DATABASE_URL (it does not — grep .github/ returns nothing).
When done: commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### D12 — Archive the pre-July half of §10
**Should-do** · M · Depends on: D7

§10 is 68 rows and **56% of `PROJECT_PLAN.md` by bytes** — the document `CLAUDE.md:33` says to
attach to every conversation. The 2026-04-23 Docker-install rows constrain nothing anyone will
do again. D2's hook now reads this table's tail as authoritative phase state, making its length
a direct per-session cost.

> **The real work is the six archived rows that are still binding rules, not history:** C++20
> (`:260`), the WSL2-native-filesystem rule (`:263`), vcpkg manifest mode with `builtin-baseline`
> as the pin (`:272`), `find_package(CONFIG REQUIRED)` (`:273`), `-Werror` from day one (`:275`),
> and the `main.cpp`/`app.cpp` split (`:278`). **Promote those into §2/§4 prose before
> archiving**, or you silently drop live rules.

**DoD:** §10 holds 17 rows, the archive 51, summing to 68; §10 opens with a pointer.
**Commit:** `Docs (Plan): archive pre-July decision-log rows, keep section 10 to the live set`

```
Main-thread task — no subagent. Attached: PROJECT_PLAN.md.

Goal: section 10 is 68 rows and 56% of the document CLAUDE.md:33 tells us to attach to
every conversation. Archive the pre-July half.

Boundary, by current line number:
 - archive :254-293 (40 rows, 2026-04-21..2026-05-05) and :295-305 (11 rows, 2026-05-05)
   into Documentation/reference/decision-log-archive.md — 51 rows
 - keep :294 (the 2026-07-26 migrate.sh row, currently misplaced among May rows) and
   :306-321 (16 rows) — 17 rows

The real work is the six archived rows that are still BINDING rules, not history. Before
archiving, promote each into the relevant prose section (2 or 4) so nothing live is lost:
 :260 C++20 · :263 repo lives on the WSL2 native filesystem · :272 vcpkg manifest mode,
 builtin-baseline is the authoritative pin · :273 find_package CONFIG REQUIRED ·
 :275 -Wall -Wextra -Wpedantic -Werror from day one · :278 main.cpp/app.cpp split so
 app.cpp can link into a test target.

Verify at the end that the two files' row counts sum to 68 and the archive preserves
original order. Add a one-line pointer at the top of section 10, and index the archive in
Documentation/README.md.
When done: commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### D13 — Add a `/phase-retro` command that verifies a phase actually closed
**Nice-to-have** · M · Depends on: D3, D7, D12

`PHASE_1_PERSISTENCE.md:421-434` names four decisions that must be logged, and nothing checked.
This time they all were — but the eight checkboxes sat unticked for two months, so the doc
under-claimed by exactly the margin a check would have caught.

The valuable part is step 3: **hunting decisions the code made that no document records**. That
is the one thing a human cannot do by reading a list — Phase 1 had three.

**Commit:** `Tooling (AI): add /phase-retro to verify a phase's checkboxes and decision rows`

```
Main-thread task — no subagent. Attached: PROJECT_PLAN.md.

Goal: add a /phase-retro slash command at .claude/commands/phase-retro.md that closes out
a phase properly. Takes a phase number, finds
Documentation/phase-prompts/PHASE_<n>_*.md, and:

 1. Verifies every section 1 checkbox against the CODE, not against the checkbox. (For
    Phase 1 all eight were true but none were ticked — the doc under-claimed for two
    months.)
 2. For each entry in the phase doc's "decisions to record" section, greps PROJECT_PLAN.md
    section 10 for a row covering it, and reports any missing. PHASE_1_PERSISTENCE.md:
    421-434 lists four; nothing ever checked them.
 3. Hunts for decisions the CODE made that no document records. This is the valuable part
    — the one thing a human cannot do by reading a list. Phase 1's three were: main.cpp:6
    calls .multithreaded() (a Phase 2 premise that appears in zero documents), the
    error-handling strategy argued at length only in a code comment, and the
    {"error": ...} response envelope that notifications_client.dart:60-62 actually parses.
 4. Ticks the verified boxes and finishes via the finish-step skill.

Mention the command in CLAUDE.md's AI-dev-setup section.
When done: commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### D14 — Trim the subagents and make main-thread the default
**Should-do** · M · Learning: **High** · Depends on: D3, D11

**A position, not a hedge: keep both files, cut them by half, and stop making them the default.**

The obvious cost is context — a subagent starts cold, so a 20-line handler change pays a fresh
read of `project_structure.md` (256 lines) plus `PROJECT_PLAN.md` (321 lines) before writing
anything. **The larger cost is specific to this project:** a subagent returns a *summary*, and
`CLAUDE.md:15-16` says the deliverable here is educational depth — "explain trade-offs, don't
just produce code". A summary compresses exactly what the project exists to produce. Every
S1–S7 prompt opens with "Use the backend subagent", so Phase 1's explanations were
systematically routed through a compressor.

But deleting them is wrong: the layer map, the skill-routing table, and the unprompted
test-coverage duty are durable routing rules worth keeping. They just aren't worth a cold
context each time.

**Commit:** `Tooling (AI): trim the subagents to routing rules and make main-thread the default`

```
Main-thread task — no subagent (fittingly). Attached: PROJECT_PLAN.md.

Goal: the subagents are being used as the default and the economics do not support it for
this repo. Restructure rather than delete.

The case. Cost 1: a subagent starts cold, so a 20-line handler change pays a fresh read of
project_structure.md (256 lines) plus PROJECT_PLAN.md (321 lines, 56% decision log) before
writing anything. Cost 2, the bigger one: a subagent returns a SUMMARY, and CLAUDE.md:15-16
says the deliverable here is educational depth — "explain trade-offs, don't just produce
code". A summary compresses exactly what this project exists to produce. Every S1-S7 prompt
in PHASE_1_PERSISTENCE.md opens with "Use the backend subagent", so Phase 1's explanations
all went through that compressor.

But the files hold real value: the layer map, the skill-routing table, and the unprompted
test-coverage duty (the only place in the repo that says to FLAG gaps in handlers/ rather
than force a unit test onto HTTP wiring). Keep those.

Do this:
 1. Delete agents/backend.md:37-42 — it restates CLAUDE.md's build loop.
 2. Keep the layer map, skill routing and test-coverage duty intact. Target under 50 lines.
 3. Same trimming pass on agents/frontend.md.
 4. Add a short CLAUDE.md subsection: main thread + Skill invocations is the DEFAULT.
    Delegate to a subagent only when the work is large, self-contained, and the artifact
    matters more than the explanation — a CI workflow rewrite, a mechanical multi-file
    refactor, a dependency sweep. Give the reason (summary compression), not just the rule.

When done: commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

## Track B — Phase 2: concurrency

### The reframe, first

`main.cpp:6` calls `.multithreaded()`. **The server has been concurrent since Phase 0**, and
because every request opens its own connection there is no shared mutable state anywhere — it
is thread-safe *by accident*. Phase 2 as §6 words it ("handle concurrent requests") is a
solution hunting for a problem.

**Phase 2 is: measure what connection-per-request costs → deliberately introduce shared state
→ learn to make that state safe.** The pool will be the first object in this codebase with a
class invariant, and therefore the first thing that can race. **Phase 2 introduces the danger
rather than removing it.**

```mermaid
flowchart LR
    A1["A1 pin vcpkg"] -.->|"only if B1 takes a new dep"| B1
    B1["B1 handlers into core<br/>+ fixture + smoke test"] --> B2["B2 per-schema isolation<br/>+ real GET/POST tests"]
    B1 --> B3["B3 /healthz + index<br/>+ LIMIT + DB timeouts"]
    B3 --> B4["B4 load harness<br/>+ committed baseline"]
    B5["B5 TSan preset,<br/>validated on a known race"] --> B6
    B4 ==>|"benchmark before<br/>the thing it justifies"| B6["B6 ConnectionPool"]
    B6 --> B7["B7 PooledConnection<br/>RAII handle"]
    B7 --> B8["B8 wire into request path"]
    B2 ==>|"guards the refactor"| B8
    B8 --> B9["B9 stress tests under TSan<br/>+ deliberate bug"]
    B9 --> B10["B10 re-measure,<br/>decide on async"]
    B4 -.->|"the before number"| B10
```

**Two deliberate inversions of habit:** `B4 → B6` builds the benchmark before the optimization
it justifies (your §10 already commits to pooling "only earning its complexity once it's
measurable" — doing B6 first makes that entry retroactive fiction, and after you've written the
pool you *will* find a way to read the numbers as vindication). `B5 → B6` validates the
sanitizer before the code it must judge — an instrument whose first run is on the code you're
anxious about cannot distinguish "no race" from "not actually instrumented".

> **B3 absorbs three cards** that a separate track also proposed (DB timeouts, `/healthz`,
> index + `LIMIT`). They are scheduled once, here, because they exist to make B4's measurement
> honest.

---

### B1 — Move `app.cpp` + handlers into `atenciosamente_core`, stand up the functional tier
**Must-fix** · M · Learning: **High** · Depends on: nothing (A1 only if you take a new dependency)

Nothing in the repo can link a handler, so the entire HTTP surface has zero coverage — and B8
rewrites exactly that surface. Your §10 (2026-04-25) justifies the `main.cpp`/`app.cpp` split as
"app.cpp can link into a test target"; it never has.

**Verified Crow 1.3.3 facts for the fixture:** `run_async()` (`app.h:665`), `stop()` (`:673`),
`wait_for_server_start()` returns `std::cv_status` (`:830`), `port()` (`:377`). **Cache the port
immediately** — `app.port()` delegates to `acceptor_.local_endpoint().port()` once started, and
`stop()` closes the acceptor, so reading it after `stop()` queries a closed socket.

```cpp
struct ApiFixture {
    crow::SimpleApp app;
    std::future<void> server;        // declared AFTER app: destroyed FIRST
    std::uint16_t port{};

    ApiFixture() {
        setup_routes(app);           // the production wiring, unmodified
        app.port(0).multithreaded(); // 0 = let the OS pick a free port
        server = app.run_async();
        if (app.wait_for_server_start(std::chrono::seconds{2}) == std::cv_status::timeout)
            throw std::runtime_error("ApiFixture: server did not start within 2s");
        port = app.port();           // MUST cache: stop() closes the acceptor
    }
    ~ApiFixture() { app.stop(); server.wait(); }
};
```

**Decisions for §10:** the HTTP client library (`cpp-httplib` recommended — header-only, one
vcpkg line; `cpr` pulls libcurl but doubles as Phase 4's outbound client; raw asio adds zero
dependencies since Crow already uses it, but you'd hand-roll response parsing inside a test);
and `Crow::Crow` becoming PUBLIC on `atenciosamente_core`, whose honest cost is that
`tests_unit` now links Crow.

**DoD:** `ctest -R '^functional/'` starts a real server on an OS-assigned port, gets
`200 "hello"` from `GET /`, shuts down with no ASan leak; two fixtures in one run don't collide.
**Commit:** `Backend (Test): add functional test tier with in-process Crow fixture`

```
Use the backend subagent to implement step B1 of Phase 2.
Attached: PROJECT_PLAN.md and ACTION_PLAN.md. Recent history:
[GIT LOG HERE]

Goal: make the HTTP layer testable. Move src/app.cpp and src/handlers/notifications.cpp
out of the atenciosamente_server source list into atenciosamente_core, promote Crow::Crow
to PUBLIC on that library, and add a third test target `tests_functional` with TEST_PREFIX
"functional/". Add an RAII ApiFixture that calls setup_routes() on a crow::SimpleApp, binds
port(0) so the OS assigns a free port, starts it with run_async(), blocks on
wait_for_server_start() and THROWS on timeout, caches app.port() immediately (app.port()
after stop() reads a closed acceptor), and in its destructor calls stop() then wait().
One test only in this step: GET / returns 200 "hello" — no database, so no isolation
question yet. Give the new target the -Wall -Wextra -Wpedantic -Werror that no test target
currently has. Drive-by: CMAKE_SOURCE_DIR at CMakeLists.txt:41 should be
CMAKE_CURRENT_SOURCE_DIR.
First propose an HTTP client library (cpp-httplib / cpr / raw asio) with trade-offs and
confirm it with me — it adds a vcpkg dependency.
Explain as you go: why the future must be declared after the app so it is destroyed first,
why RAII fixtures beat setup/teardown functions in Catch2, and what PUBLIC vs PRIVATE
linkage actually propagates.
When done: record the HTTP-client choice and the Crow-goes-PUBLIC trade-off in
PROJECT_PLAN.md §10 and commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### B2 — Give the functional tier its own Postgres schema; test the real GET/POST
**Must-fix** · M · Learning: **High** · Depends on: B1

Functional tests go through the POST handler, which **commits**, so per-test rollback is
unavailable. Because `port(0)` works they run in parallel, so truncate-between-tests is also out
— one case would wipe rows another is asserting on.

> **Do not set the schema with `setenv`.** Worker threads call `std::getenv` per request
> (`connection.cpp:21`), and mutating the environment underneath them is a real data race TSan
> will flag in B9. Put it in the conninfo: `options='-c search_path=<schema>'` — the same seam
> the pool will use in B8.

**DoD:** `ctest -R '^functional/' -j8` passes repeatedly; `SELECT count(*)` in the *public*
schema is unchanged before and after a run; a third CI job runs the tier.
**Commit:** `Backend (Test): isolate functional tests in a per-run Postgres schema`

```
Use the backend subagent to implement step B2 of Phase 2.
Attached: PROJECT_PLAN.md and ACTION_PLAN.md. Recent history:
[GIT LOG HERE]

Goal: make the functional tier database-isolated and test the real request path. The POST
handler commits (handlers/notifications.cpp:113), so per-test transaction rollback is
unavailable here, and the tests run in parallel because port(0) works, so truncating
between tests is unavailable too. Have the fixture CREATE SCHEMA with a pid+random name,
apply migrations/*.sql into it, and DROP SCHEMA ... CASCADE on teardown; route connections
to it via options='-c search_path=<schema>' in the conninfo. Do NOT use setenv: worker
threads call std::getenv per request in db/connection.cpp:21, and mutating the environment
under them is a real data race TSan will flag later.
Add functional tests for GET 200 + Content-Type, POST 201, POST 400 on malformed JSON, and
POST-then-GET round-trip. Add a third CI job mirroring the integration job's shape, and
update the Mermaid diagram in .github/workflows/README.md in the same commit.
Use the backend-add-test skill.
Explain as you go: why commit-then-isolate is a fundamentally different problem from
rollback isolation, and what a Postgres schema actually is versus a database.
When done: record the isolation strategy, the migrations-path mechanism, and the no-setenv
rule in PROJECT_PLAN.md §10 and commit in `Scope (Tag): summary` style (no body, no
trailers).
```

---

### B3 — Make the system measurable: `/healthz`, composite index, `LIMIT`, DB timeouts
**Must-fix** · M · Depends on: B1

Without these, B4 measures the wrong thing four ways: no `/healthz` means framework and DB cost
are one number; no index means you measure a seq-scan plus sort; no `LIMIT` means you measure
serializing the whole table; **no `statement_timeout` means p99 has no upper bound at all**.
That last is also the failure the pool re-shapes — N wedged queries pin all N workers today, and
all N pool slots tomorrow.

The index must be **composite and match the sort exactly** — `(created_at DESC, id DESC)` —
because a single-column index still sorts on ties. Timeouts go in the conninfo as literal
constants and deliberately **not** through `escape_conninfo_value()`, which exists for untrusted
env *values*.

**DoD:** `EXPLAIN ANALYZE` shows an index scan with no `Sort` node; `GET /healthz` returns 200
with the `db` container stopped, asserted by a functional test.
**Commit:** `Backend (Feat): add /healthz, created_at index, result limit and DB timeouts`

```
Use the backend subagent to implement step B3 of Phase 2.
Attached: PROJECT_PLAN.md and ACTION_PLAN.md. Recent history:
[GIT LOG HERE]

Goal: make the backend measurable before Phase 2 benchmarks it. Four changes: (1) a
GET /healthz handler that touches NO database, so the load test can separate framework
cost from DB cost, with a functional test that passes while the db container is stopped;
(2) a migration adding a composite index on notifications (created_at DESC, id DESC) —
composite because ORDER BY created_at DESC, id DESC over a single-column index still sorts
on ties; (3) a LIMIT on get_all()'s query, a named constant, NOT a query parameter
(a caller-supplied limit is pagination, which stays deliberately deferred); (4)
connect_timeout=3 and options='-c statement_timeout=5000' in the conninfo in
db/connection.cpp, as literal constants that do NOT go through escape_conninfo_value() —
that helper exists for untrusted env values, not for text I wrote.
Keep GET / as it is. A readiness probe that checks the DB is a separate, later endpoint —
name the liveness/readiness distinction and defer the second one.
Use the backend-add-endpoint and backend-add-migration skills.
Explain as you go: what a composite index buys over a single-column one for this exact
ORDER BY, and why an unbounded statement is worse under concurrency than under serial load
(Crow's 5s HTTP timeout kills the client socket but leaves the worker blocked in libpq).
Show me EXPLAIN ANALYZE before and after against a seeded table.
When done: record the LIMIT value, the liveness-vs-readiness split, and the timeout values
in PROJECT_PLAN.md §10 and commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### B4 — Load harness and a committed baseline
**Must-fix** · M · Learning: **High** · Depends on: B3

This is the card that makes the rest of Phase 2 honest. Without a number recorded *before* the
pool exists, §10's "pooling only earns its complexity once it's measurable" becomes something
you wrote and then ignored. It is also the only way B10 can conclude "no measurable
improvement" — a legitimate outcome that is impossible to reach without a before.

**The number that matters** is not any absolute figure — it's the **delta between `/healthz` and
`GET /notifications` at concurrency 64**. That subtracts out your laptop, the container and
Crow, leaving the thing the pool is supposed to move.

> Run against the **`ci` (Release) preset, never `dev`** — ASan instrumentation makes the
> numbers meaningless, and this is the easiest way to invalidate the whole exercise.

Record `nproc` (because `.multithreaded()` = `hardware_concurrency()`), the tool and version,
the exact command, the seeded row count, the preset and the commit SHA.

**DoD:** percentile tables for three scenarios at concurrency 1/8/64; `BASELINE.md` committed
with the environment block filled in; the headline delta stated as one named number.
**Commit:** `Backend (Perf): add load-test harness and record the pre-pool baseline`

```
Use the backend subagent to implement step B4 of Phase 2.
Attached: PROJECT_PLAN.md and ACTION_PLAN.md. Recent history:
[GIT LOG HERE]

Goal: measure connection-per-request before replacing it. Add backend/benchmarks with a
seed script (enough rows that B3's index matters), a run.sh driving three scenarios —
GET /healthz, GET /notifications, POST /notifications — at concurrency 1, 8 and 64, and a
committed BASELINE.md. Report p50/p90/p99/max and throughput, not averages: handshake cost
lives in the tail. Run against the ci (Release) preset binary, never dev — ASan makes the
numbers meaningless. Record nproc, the tool and version, the exact command, the seeded row
count and the commit SHA, because .multithreaded() resolves to hardware_concurrency() and
the numbers are machine-specific. Dockerfile.dev ships no load tool today, so add one.
First propose the tool (wrk / hey / ab) and confirm it with me — note ab is widely known to
misreport under concurrency.
The headline number is the DELTA between /healthz and GET /notifications at concurrency
64 — that subtracts out the machine and Crow and leaves DB connection cost, the only thing
the pool can move.
Explain as you go: why percentiles beat averages for latency, what coordinated omission is,
and why we are benchmarking before building the optimization rather than after.
When done: record the tool choice, where baselines live, and the reproducibility fields in
PROJECT_PLAN.md §10 and commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### B5 — Add a TSan preset and validate it against a deliberate, known race
**Must-fix** · S · Learning: **High** · Depends on: nothing

A passing concurrent test is weak evidence — races are probabilistic, and a test that passes
1000 times can fail on the 1001st on a different machine. Without a sanitizer you are not
testing concurrency, you are sampling it. TSan cannot coexist with ASan, so this is a **third**
preset.

**This card's deliverable is a known-positive.** Write a three-line test with two threads
incrementing a plain `int`, confirm TSan reports it with a readable stack, then **delete it in
the same commit** and record that it was seen. Every concurrency claim for the rest of the phase
then rests on a validated instrument.

> Note the asymmetry that shapes B9: Crow is header-only and gets instrumented; `libpqxx-8.0.a`
> is a prebuilt static archive and does **not**.

**DoD:** the tsan preset runs existing suites clean; configuring with both sanitizer options on
fails with a clear message; the throwaway race was observed and its report recorded.
**Commit:** `Backend (Build): add TSan preset for the concurrency phase`

```
Use the backend subagent to implement step B5 of Phase 2.
Attached: PROJECT_PLAN.md and ACTION_PLAN.md. Recent history:
[GIT LOG HERE]

Goal: add a third CMake preset `tsan` (ENABLE_TSAN option, -fsanitize=thread,
RelWithDebInfo) that hard-errors at configure time if ENABLE_SANITIZERS is also on — TSan
and ASan cannot coexist, and a message(FATAL_ERROR) turns a confusing startup abort into
one sentence. Apply the flags to atenciosamente_core, the server and all test targets,
exactly as the existing ENABLE_SANITIZERS block does. Set TSAN_OPTIONS=halt_on_error=1 so
a detected race fails the test instead of printing into a log nobody reads.
Then VALIDATE the instrument before trusting it: write a throwaway test with two threads
incrementing a plain int, show me TSan's report, and delete that test in the same commit —
note in the conversation that it was seen. A sanitizer whose first run is on the code
you're worried about can't tell "no race" from "not actually instrumented".
Explain as you go: what TSan's happens-before shadow memory tracks, why header-only Crow
gets instrumented but the prebuilt libpqxx-8.0.a archive does not, and what that asymmetry
means for what TSan can and cannot see here.
When done: record the preset, the ASan/TSan exclusivity, the halt_on_error choice and a
suppressions policy (every entry must carry a one-line reason) in PROJECT_PLAN.md §10 and
commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### B6 — Build `ConnectionPool`: fixed-size, explicit `acquire`/`release`
**Must-fix** · M · Learning: **High** · Depends on: B4, B5

This is the point of Phase 2. It is the **first class in the codebase that earns being a class**,
because it is the first type with an invariant — *a checked-out connection is not in the free
list* — and an invariant is the only thing a mutex has to protect.

> This card deliberately ships an **unsafe API**: `take()` hands back a connection and the caller
> must remember to `give_back()`. B7 fixes that with RAII. The split is the lesson — write the
> leak-prone version, feel why it's leak-prone, then watch a destructor solve it.

Use the **predicate overload** of `cv_.wait_for`, not the bare one: it re-checks on every wake,
so a spurious wakeup — or losing the race to another waiter the same notify woke — loops instead
of returning from an empty pool. Make `give_back` `noexcept` (B7's destructor calls it, and a
throwing destructor during unwinding is `std::terminate`). Notify **after** releasing the lock.

**Decisions for §10:** pool size source (`DB_POOL_SIZE`, read once at construction, defaulting to
the Crow worker count — and note the physics: smaller than the worker count means workers block
on each other, larger wastes Postgres slots against a default `max_connections` of 100);
exhaustion behaviour (**bounded wait then throw**, not block forever — a blocking pool converts a
DB stall into a silent total hang); eager construction at startup; and restating §4.3 as **"no
service objects; no classes without invariants"**.

> Honest caveat worth recording: with pool size equal to the worker count, **HTTP traffic alone
> can never exhaust the pool** — each worker holds at most one. That path is dead code until
> Phase 3's background worker also draws from it, so the test must construct a deliberately
> undersized pool to reach it.

**DoD:** N threads on a size-1 pool serialize and all succeed; an undersized pool throws after
the timeout rather than hanging; free count returns to N. Clean under `tsan`.
**Commit:** `Backend (Feat): add fixed-size ConnectionPool with mutex and condition_variable`

```
Use the backend subagent to implement step B6 of Phase 2.
Attached: PROJECT_PLAN.md and ACTION_PLAN.md. Recent history:
[GIT LOG HERE]

Goal: add src/db/connection_pool.{hpp,cpp} — a fixed-size pool of already-open
pqxx::connections guarded by a std::mutex and std::condition_variable, with the explicit
invariant "a connection is in the free list or checked out, never both, never neither".
This step ships a DELIBERATELY unsafe API: take() / give_back(), caller must remember to
return it. B7 replaces that with RAII — I want to feel the leak-prone version first.
Use the predicate overload of cv_.wait_for, not the bare one. Make give_back noexcept (a
destructor will call it in B7). Notify after releasing the lock, not while holding it.
Construct all connections eagerly in the constructor so a bad password is a startup crash,
not a 500 on request #1. Read the size from DB_POOL_SIZE once at construction, defaulting
to the Crow worker count — read that from std::thread::hardware_concurrency() directly
rather than through Crow's getter, which narrows to uint16_t. Exhaustion: bounded wait_for,
then throw a typed PoolExhausted — not block forever. Develop and run under the tsan preset
from the first build. Don't wire it into any handler yet — that's B8.
Use the backend-add-test skill.
Explain as you go: why a mutex alone is not enough and what the condition_variable adds,
what a spurious wakeup is and why the predicate overload is the fix, why the lost-wakeup
problem forces the condition to be checked under the same lock that protects it, and why
this is the first type in the codebase that earns being a class rather than free functions.
When done: record the pool size source and default, the exhaustion behaviour (with the
honest note that HTTP alone can't exhaust a pool sized to the worker count), the eager
construction, and the amendment of §4.3 to "no classes without invariants" in
PROJECT_PLAN.md §10, and commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### B7 — `PooledConnection`: the move-only RAII checkout handle
**Must-fix** · M · Learning: **High** · Depends on: B6

B6's API leaks a connection on every exception path — and the handlers deliberately let
exceptions unwind into Crow's default handler, so **every 500 leaks a slot** until the pool is
empty and the server deadlocks with no error. A destructor is the only mechanism in C++ that
runs on both the normal and the unwinding path, which is exactly why RAII is the answer and a
`try/catch` at each call site is not.

The `std::exchange(o.pool_, nullptr)` in the move constructor is the whole trick: a moved-from
handle must be **destructible but inert**, or the connection is returned twice and the free list
holds two entries pointing at one connection — the exact bug B9 injects on purpose.

> **Write both versions.** The entire class is replaceable by
> `std::unique_ptr<pqxx::connection, PoolDeleter>` in about six lines, which gives you move-only,
> correct moved-from behaviour and deleted copies for free. Write the hand-rolled one first to
> learn the mechanics, then the `unique_ptr` one beside it, and decide which ships. Either answer
> is defensible; the decision must be recorded.

**DoD:** a test proves the connection returns when the scope exits via a **thrown exception**; a
moved-from handle's destruction doesn't double-return; the copy constructor fails to compile.
**Commit:** `Backend (Feat): add move-only RAII PooledConnection checkout handle`

```
Use the backend subagent to implement step B7 of Phase 2.
Attached: PROJECT_PLAN.md and ACTION_PLAN.md. Recent history:
[GIT LOG HERE]

Goal: replace B6's manual take()/give_back() with a move-only RAII handle,
PooledConnection, whose destructor returns the connection to the pool. Copy deleted; move
must leave the source inert via std::exchange(pool_, nullptr) so a moved-from handle's
destructor is a no-op; move-assignment must release its own connection first and handle
self-assignment. Make ConnectionPool::acquire() return one, and make the raw
take()/give_back() private with PooledConnection a friend, so the unsafe API is no longer
reachable. Then, in the same session, ALSO write the alternative —
std::unique_ptr<pqxx::connection, PoolDeleter> — show me both side by side, and let's
decide together which ships.
Use the backend-add-test skill. The key test is that a connection returns to the pool when
the enclosing scope exits via a THROWN exception, because the handlers deliberately let
exceptions unwind into Crow's default handler, so every 500 would otherwise leak a slot.
Explain as you go: the rule of five and why this type only needs three of them, what
"valid but unspecified" means for a moved-from object and why ours is deliberately
stronger, why give_back must be noexcept, and where std::unique_ptr's custom deleter gives
you all of this for free.
When done: record the handle-vs-unique_ptr choice, the moved-from contract, and the
broken-connection policy (discard and reopen if is_open() is false — or its deferral and
trigger) in PROJECT_PLAN.md §10 and commit in `Scope (Tag): summary` style (no body, no
trailers).
```

---

### B8 — Wire the pool into the request path
**Must-fix** · M · Learning: **High** · Depends on: B7 **and B2**

This is where the phase pays off and where it collides with your own principles.
**`get_all(pqxx::work&)` and `insert(pqxx::work&, ...)` do not change** — that is the dividend of
the §10 (2026-07-26) decision to take a transaction rather than a connection, and it's worth
saying out loud when it lands.

**The ownership question, all three answers:** (a) a function-local `static` behind a `db::pool()`
accessor — handlers stay zero-argument and it's *correct* (C++11 guarantees thread-safe init),
but it's a global the test fixture can't repoint at its own schema; (b) **construct in `main.cpp`
and thread it through `setup_routes(SimpleApp&, ConnectionPool&)`** — recommended, because it's
the only option that keeps B2's fixture able to inject a test pool, which is the only reason the
refactor is guarded at all; (c) a Crow middleware in the request context — idiomatic Crow, but
couples handlers to Crow's context machinery.

> **On the apparent collision with §10's "handler registered as a plain free function, not a
> lambda":** option (b) forces a one-line forwarding lambda. That *looks* like a violation. It
> isn't — the recorded **justification** was "named functions are easier to test and trace in
> stack dumps", and a lambda that does nothing but forward to a named handler preserves both.
> A principle stated as a rule about syntax and the same principle stated as a rule about
> properties diverge here, and only one was ever the point. **Amend the row rather than quietly
> breaking it.**

**DoD:** every B2 functional test passes unchanged; `make_connection()` appears nowhere in
`handlers/`; a GET that throws returns 500 *and* the pool's free count returns to full.
**Commit:** `Backend (Refactor): serve requests from the connection pool`

```
Use the backend subagent to implement step B8 of Phase 2.
Attached: PROJECT_PLAN.md and ACTION_PLAN.md. Recent history:
[GIT LOG HERE]

Goal: make both handlers take their connection from the pool instead of calling
make_connection() per request (handlers/notifications.cpp:46 and :101). The repository
signatures get_all(pqxx::work&) and insert(pqxx::work&, title, body) MUST NOT change —
that is the payoff of the 2026-07-26 §10 decision to take a transaction rather than a
connection, and I want that called out when it lands.

Before writing code, walk me through the pool-ownership options and let's decide together:
(a) a function-local static behind a db::pool() accessor — handlers stay zero-argument, but
it's a global the test fixture can't repoint at its own schema; (b) construct it in
main.cpp and thread it through setup_routes(crow::SimpleApp&, ConnectionPool&), with
handlers taking a ConnectionPool& and routes registered via a one-line forwarding lambda;
(c) a Crow middleware in the request context. I lean (b). Note that (b) appears to
contradict the §10 2026-04-26 row "handler registered as a plain free function, not a
lambda" — but that row's stated justification was "easier to test and trace", which a
forwarding lambda preserves. Amend the row rather than breaking it silently.

Also map PoolExhausted to a 503. Crow already has a typed-exception-to-status channel
(crow::bad_request -> 400, routing.h:1855) that this codebase has never used — show me it.
Keep make_connection(): migrations and the integration tests still use one-off connections,
and the pool's constructor uses it internally. Replace the Phase 2 TODO at
connection.cpp:50-60 with a pointer to connection_pool.hpp.
The functional tests from B2 must pass UNCHANGED; that's what makes this refactor safe.
Explain as you go: why dependency injection and "free functions over classes" pull in
opposite directions here, and which should win and why.
When done: record the ownership choice, the amended lambda row, the fate of
make_connection(), and the 503 mapping in PROJECT_PLAN.md §10 and commit in
`Scope (Tag): summary` style (no body, no trailers).
```

---

### B9 — Prove the pool is race-free: stress tests under TSan, plus a deliberate bug
**Must-fix** · M · Learning: **High** · Depends on: B8

B5 validated TSan on a toy race; this validates it on *your* data structure, which is harder —
TSan can see a race on your free list, but `libpqxx-8.0.a` is uninstrumented, so it may not see
the *consequence* of a double-checkout. **Knowing which of your bugs the tool can and cannot see
is the actual skill.**

> Use `std::jthread`, not `std::thread` — and this is not style. If a Catch2 `REQUIRE` fails, the
> stack unwinds past any un-joined `std::thread`, whose destructor calls `std::terminate`, and
> **you lose the failure message entirely**. `jthread` joins in its destructor.

**The deliberate bug, done properly:** remove the `std::exchange(o.pool_, nullptr)` from the move
constructor so a moved-from handle double-returns, run under TSan, and record **exactly what you
see** — a clean race report, a corrupted free list, an inexplicable libpqxx error from inside the
uninstrumented archive, or 1000 passes and a failure on the 1001st. Then revert. **That log entry
is worth more than the test.**

**Assert invariants, not timings:** free count returns to N; no connection held by two threads at
once; every response is 200/201 or a clean 503, never a 500 or a truncated body.

> **The flakiness rule, write it down before you're tired and it fails at 1am:** a concurrency
> test that fails intermittently is **never** retried or marked flaky. It is a real bug that has
> not been found yet.

**Commit:** `Backend (Test): add concurrency stress tests for the pool under TSan`

```
Use the backend subagent to implement step B9 of Phase 2.
Attached: PROJECT_PLAN.md and ACTION_PLAN.md. Recent history:
[GIT LOG HERE]

Goal: prove the pool is race-free rather than asserting it. Add a functional concurrency
test driving hundreds of simultaneous GET and POST requests at the real in-process server
using std::jthread — NOT std::thread: if a Catch2 REQUIRE fails and the stack unwinds past
an unjoined std::thread, its destructor calls std::terminate and the failure message is
lost. Use std::stop_source/std::stop_token to end a timed run cooperatively. Assert
invariants, not timings: the free count returns to N afterwards, no connection is ever held
by two threads at once, and every response is 200/201 or a clean 503 — never a 500 or a
truncated body. Run under the tsan preset and add a CI job that does the same.

Then the important part: deliberately break it. Remove the std::exchange(o.pool_, nullptr)
from PooledConnection's move constructor so a moved-from handle double-returns, run the
stress test under TSan, and tell me EXACTLY what you observe — a clean race report, a
corrupted free list, a confusing libpqxx error, or nothing at all. Then revert. I want to
know which of my own bugs this tool can and cannot see, given that Crow is header-only
(instrumented) but libpqxx-8.0.a is a prebuilt archive (not).
Use the backend-add-test skill.
Explain as you go: why std::jthread's auto-join matters specifically inside a test, how
stop_token cooperative cancellation differs from killing a thread, and why an intermittent
concurrency failure is a bug rather than flake.
When done: record what the tests assert, the TSan CI job, and the "never retry a flaky
concurrency test" rule in PROJECT_PLAN.md §10 and commit in `Scope (Tag): summary` style
(no body, no trailers).
```

---

### B10 — Re-measure, compare, and decide on async handlers
**Should-do** · M · Learning: **High** · Depends on: B9, B4

Closes the loop B4 opened, and teaches the thing most engineers never learn: **"no measurable
improvement" is a good outcome, and manufacturing a win to justify work already done is the
failure mode.**

Two honest outcomes, both of which must be written before the run: the delta shrank materially
(say by how much), **or** it barely moved — in which case say so plainly and reason about why. A
local Postgres over a loopback socket has a handshake cost in hundreds of microseconds, easily
invisible next to Crow's per-request work. The pool was still right to build, because **the
lesson was mutexes and RAII, not milliseconds.** That will be the most intellectually honest row
§10 contains.

> **Write the async decision rule *before* you see the numbers.** Something falsifiable: adopt
> async handlers only if, at concurrency 64, worker threads are demonstrably blocked in libpq for
> the majority of wall time **and** p99 exceeds target. Writing the rule afterwards is how a
> foregone conclusion gets dressed as analysis. Expected answer: **no** — libpqxx is synchronous,
> so "async handlers" means replacing the database client, not restructuring Crow.

**Commit:** `Docs (Phase 2): record post-pool benchmarks and close out the concurrency phase`

```
Use the backend subagent to implement step B10 of Phase 2.
Attached: PROJECT_PLAN.md and ACTION_PLAN.md. Recent history:
[GIT LOG HERE]

Goal: close out Phase 2 honestly. Re-run benchmarks/run.sh unchanged — same machine, same
ci preset, same seeded row count — and put the after table beside B4's before table in
BASELINE.md, with deltas computed. The headline is the /healthz vs GET /notifications delta
at concurrency 64.

BEFORE you look at the numbers, write down the rule that decides whether Phase 2 adopts
async handlers, and show it to me. Then apply it. I expect NO — libpqxx is a synchronous
driver, so "async handlers" would mean replacing the database client, not restructuring
Crow — but the reason must be recorded either way. If the pool produced no measurable
improvement, say that plainly: the lesson was mutexes, condition variables and RAII, not
milliseconds, and a manufactured win would be worse than an honest null result.
Finally, rewrite §6's Phase 2 row to describe what actually happened (the phase was
"measure, then deliberately introduce shared state, then make it safe", not "handle
concurrent requests" — the server has been multithreaded since main.cpp:6 in Phase 0), and
re-frame §6's Phase 3 row around lifecycle and cooperative shutdown, since B9 already
covered background threads. Name the two Phase 3 "design patterns" concretely while you're
there: Strategy for delivery channel, and a producer/consumer queue.
Explain as you go: why a null result is a legitimate engineering outcome, and what
"asynchronous handler" would actually require given a synchronous database driver.
When done: record the async decision and its pre-registered rule in PROJECT_PLAN.md §10
and commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

## Track C — API hardening

Four cards. Three others originally proposed here (DB timeouts, `/healthz`, index + `LIMIT`)
were folded into **B3**, where they belong.

> **What these DoDs are worth today.** Every card here ships behaviour **no automated test can
> currently reach** — `app.cpp` and `handlers/` compile only into the executable, so status
> codes, headers, error bodies and routing have zero coverage by construction. The `curl` checks
> are a one-shot human check, not a regression test. **B1/B2 are what convert this track into
> real protection.**

---

### C2 — One error envelope for the whole API
**Should-do** · S · Learning: **High** · Depends on: nothing (do before B2 so the tier pins it)

Your API speaks two error formats and only one is yours: handler 400s are JSON, while 404, 405
and 500 are empty `text/plain` from Crow's defaults. This is a **live user-facing defect** —
`notifications_client.dart:58-67` lifts `decoded['error']` and silently falls back to "Failed to
create notification (404)", so every routing or server error shows a status code instead of a
reason.

~20 lines, and it also gives you Crow's typed `crow::bad_request` → 400 channel
(`routing.h:1855-1862`), which this codebase has never used. Both 404 and 405 are reachable via
`CROW_CATCHALL_ROUTE` because `Router::find()` sets `catch_all = true` alongside the code
(`routing.h:1721`, `:1729`). Working code is in [`PROJECT_ANALYSIS.md`](PROJECT_ANALYSIS.md) §2.

**DoD:** `curl -i localhost:8080/nope`, `curl -i -X DELETE .../notifications`, and a forced throw
all return `application/json` with an `{"error": ...}` body; the 500 says "internal server error"
and the real message appears only in the log.
**Commit:** `Backend (Fix): return JSON errors for 404, 405 and 500`

```
Use the backend subagent to give the API a single error envelope.
Attached: PROJECT_PLAN.md and ACTION_PLAN.md. Recent history:
[GIT LOG HERE]

Goal: in backend/src/app.cpp, register app.exception_handler(...) and
CROW_CATCHALL_ROUTE(app)(...) so 404, 405 and 500 return the same {"error": "..."} JSON
shape the 400 path already uses. The catchall must read the status code Crow's
Router::find() already set (405 vs 404) rather than assuming 404. Never echo an exception's
message to the client on a 500 — log it with CROW_LOG_ERROR and return a fixed "internal
server error"; DO echo crow::bad_request's message, because that one is the client's fault.
Explain the concept: why `throw;` is only legal inside the exception_handler callback, how
Crow's default_exception_handler already distinguishes crow::bad_request from a generic
std::exception, and why leaking internal messages on a 500 is the classic
information-disclosure bug. Note in a comment that nothing tests this yet — the functional
tier is what will pin it.
When done: record the {"error": ...} envelope as the API's error contract in
PROJECT_PLAN.md §10 (the Dart client already depends on it and no document says so) and
commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### C5 — Trim and length-cap `title` and `body`
**Should-do** · S · Depends on: nothing

`create_notification_request.cpp:25-27` rejects only *empty* strings, so
`{"title":"   ","body":"   "}` returns **201** and stores a blank notification you can see in
the app and can't explain. The columns are unbounded `TEXT`, so a multi-megabyte title is
accepted and persisted forever — **this is the half of the body-size problem you can actually
enforce** (see the register below for why the other half can't be done in Crow).

**DoD:** whitespace-only returns 400; an over-length title returns 400; unit tests cover trim,
both caps and the boundaries; the CHECK constraint exists and never fires from the HTTP path.
**Commit:** `Backend (Feat): trim and length-cap notification title and body`

```
Use the backend subagent to tighten notification input validation.
Attached: PROJECT_PLAN.md and ACTION_PLAN.md. Recent history:
[GIT LOG HERE]

Goal: in parse_create_notification_request(), trim leading/trailing whitespace from title
and body before the empty check (so "   " is rejected, and the trimmed value is what gets
stored), and reject title longer than 200 characters or body longer than 4000. Propose
those two numbers to me before implementing — they are product decisions, not technical
ones. Add a migration with matching CHECK constraints as a backstop, and comment that the
CHECK should never fire from the HTTP path: if it ever does, that's a bug and a 500 is the
honest answer.
Use the backend-add-test skill for the unit tests and backend-add-migration for the
constraint.
Explain the concept: how to trim with std::string_view and find_first_not_of /
find_last_not_of without allocating, and the trade-off of validating in two places
(domain + database) — defense in depth versus two definitions of one rule that can drift.
Also note that "characters" here means bytes, and what that means for a UTF-8 title of
emoji.
When done: record the caps and the two-layer validation decision in PROJECT_PLAN.md §10 and
commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### C6 — Put the API behind `/v1`
**Should-do** · S · Learning: Low · Depends on: C2 (catchall covers the new prefix), B3 (probes
stay *outside* `/v1` — they're operational, not API)

Five lines today, a permanent compatibility shim later, and **the deadline is invisible**: it is
not "Phase 4", it is the specific day you stop running `flutter run` from source and install a
release APK you won't rebuild. You will not notice that day. Right now your only client compiles
from this same repo, so the change is atomic and free.

> **This commit touches backend and mobile together.** That's the exception, not a slip —
> splitting it leaves `main` broken between the two commits, and it is one contract change.

**DoD:** `GET /v1/notifications` works; `GET /notifications` returns C2's JSON 404; `/healthz`
and `/readyz` unprefixed; the app still lists and creates.
**Commit:** `Backend (Feat): prefix the API with /v1`

```
Use the backend subagent to version the API under /v1.
Attached: PROJECT_PLAN.md and ACTION_PLAN.md. Recent history:
[GIT LOG HERE]

Goal: move both /notifications routes in backend/src/app.cpp to /v1/notifications and
update the two Uri.parse calls in mobile/.../lib/api/notifications_client.dart to match.
Keep /healthz, /readyz and / unprefixed — operational endpoints are not part of the API
contract and should not move when the API version does. This is one atomic commit across
backend and mobile on purpose: splitting it would leave main broken in between.

Explain the concept, and run an experiment rather than assuming the answer. CROW_ROUTE is
not an ordinary call — on this build it expands (app.h:70) to
app.template route<crow::black_magic::get_parameter_tag(url)>(url), so the path is a
TEMPLATE ARGUMENT that Crow parses at compile time to derive the handler's parameter types.
get_parameter_tag takes a const_str (utility.h:219) whose only constructor is
template<unsigned N> const_str(const char (&arr)[N]) — an array reference, with a
static_assert reading "not a string literal". So try defining the prefix once as
`constexpr const char* kPath = "/v1/notifications";` and again as
`constexpr char kPath[] = "/v1/notifications";`, and show me which compiles and why. The
pointer form cannot deduce N; the array form should. Tell me what the error looks like.
If the array form works, use it; if not, use plain literals and tell me what blocked it.
Also explain what versioning actually buys: not "supporting two versions", but keeping the
option to break the contract once a client exists that you cannot rebuild.
When done: record that the API is versioned by URL path from here on, with operational
endpoints excluded, plus the outcome of the route-path experiment, in PROJECT_PLAN.md §10
and commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

### C7 — Take the listen port from env and report startup failures
**Nice-to-have** · S · Depends on: nothing · *(overlaps A8 — do them together)*

`main.cpp:3-6` hardcodes 8080 and has no try/catch, contradicting §4.1 in the single easiest
place to honour it. Low stakes — ten confused minutes, once — but it's ten lines and it's the
natural home for A8's boot-time config validation.

**DoD:** `PORT=9000` listens on 9000; unset defaults to 8080; a taken port prints one clear line
and exits non-zero.
**Commit:** `Backend (Feat): take the listen port from env and report startup failures`

```
Use the backend subagent to harden the server entry point.
Attached: PROJECT_PLAN.md and ACTION_PLAN.md. Recent history:
[GIT LOG HERE]

Goal: in backend/src/main.cpp, read the listen port from a PORT env var defaulting to 8080
(§4.1 says config comes from env, and this is the one place still ignoring it), and wrap
the body in try/catch so a startup failure prints a single clear message to stderr and
returns a non-zero exit code. Keep main() thin — no logic beyond config, wiring and the
catch. Coordinate with the startup config-validation work if that hasn't landed yet; these
two belong in the same file and probably the same commit.
Explain the concept: what actually happens today when an exception escapes main — that it
is std::terminate and abort, not an orderly unwind, and that destructors of automatic
objects are NOT guaranteed to run — and why main is one of the few places a catch(...) is
correct rather than a code smell. Mention what exit code convention you chose and why.
When done: record the PORT env var in PROJECT_PLAN.md §10 and commit in
`Scope (Tag): summary` style (no body, no trailers).
```

---

## Phases 3 and 4 — framed so the learning survives

### Phase 3 — Scheduled notifications

§6 currently promises "background threads, design patterns, clock abstraction". Two of those
three are labels, not goals.

**Decisions to make, in order:** where "scheduled" lives (add `deliver_at` / `delivered_at`
columns rather than a separate table — don't skip a rung); whether `GET /notifications` returns
scheduled-but-undelivered rows (**it must not**, or the phase ships nothing visible: you create
a notification for 60 seconds hence, watch it not appear, and watch it appear — that's the
walking skeleton); how the worker claims work (**polling + `FOR UPDATE SKIP LOCKED`**, not
`LISTEN`/`NOTIFY`, which can't express "wake me at a future time"); delivery semantics (decide
now that delivery is **at-least-once with an idempotent marker** — Phase 4 will be grateful,
because FCM is also at-least-once); and worker lifetime.

**Name the two patterns concretely, or the label dissolves:**

1. **The claim loop** (transactional outbox / `FOR UPDATE SKIP LOCKED`) — teaches row-level
   locking, why `SKIP LOCKED` rather than `NOWAIT`, and that **your queue is a table**, which is
   the correct architecture at this scale.
2. **Scoped cooperative cancellation** (`std::jthread` + `std::stop_token`) — the naive worker
   sleeps 10 seconds between polls, so `Ctrl-C` takes up to 10 seconds. The fix is
   `std::condition_variable_any::wait_for(lock, stop_token, interval, pred)`, the C++20 overload
   that exists precisely so a sleeping thread can be woken by a stop request. Genuinely obscure,
   genuinely important, and the phase's best C++ moment.

> **Deliberately *not* Strategy, and not a producer/consumer queue — both move to Phase 4.**
> Strategy with one implementation is the premature abstraction this codebase has admirably
> avoided; extracting an interface from two *real* implementations in Phase 4 teaches more than
> inventing one from zero. And a producer/consumer queue in Phase 3 is invented work: the
> producer is Postgres, and an in-process queue duplicates the durable queue you already have
> while losing work on restart.

**The clock, and why `now()` is the obstacle.** Your only time source is `now()` embedded in SQL
text — server-side, transaction-scoped, not addressable from a test. **The single move that makes
a clock abstraction possible is taking `now()` out of the SQL:** `WHERE deliver_at <= $1 AND
delivered_at IS NULL`, cutoff passed as a bind parameter. Once time is a parameter rather than a
side effect of executing a statement, the worker owns the clock and a test passes a fixed
`time_point`. *You cannot inject a clock while `now()` is in the query text* — that sentence is
the whole lesson, and it's why the "timestamp parameter" idea rejected for testing in A4 becomes
**correct** here: it stops being a test-only widening and becomes the production seam.

Also teach: `steady_clock` must **never** be used for wall-clock scheduling (no epoch
relationship to `deliver_at`), and `system_clock` must **never** be used to measure the poll
interval (it can jump backwards on an NTP correction and your worker sleeps for an hour).

**§6 rewrite:** *Goal* — background worker claims due notifications and delivers them in-app;
`GET /v1/notifications` hides undelivered future ones; delivery is at-least-once with an
idempotent marker. *Teaches* — `std::jthread` + `std::stop_token` cooperative shutdown; claim
loop with `FOR UPDATE SKIP LOCKED`; **time as an injected parameter — `now()` leaves the SQL**.
Delete the words "design patterns" and "background threads".

### Phase 4 — Push notifications

**What plainly cannot be done offline or solo** — state this before the phase starts:

- A Google account and a real **Firebase project** (free tier is enough), producing a
  `google-services.json` for the app and a **service-account JSON private key** for the server.
- **FCM HTTP v1 only.** The legacy server-key API is gone. v1 needs an OAuth2 bearer token you
  mint yourself: build a JWT, sign RS256 with the service-account key, exchange it at Google's
  token endpoint, cache for its ~1 hour lifetime. **That is the actual credential lesson.**
- **A physical Android device** — FCM needs Play services; a plain AOSP emulator receives nothing.
- **APNs is a money-and-hardware gate, not a code gate** (paid Apple account + a real iPhone; the
  simulator receives no push). **Scope Phase 4 to Android/FCM and say so in §6.**
- **Deploy rung 2**, if the credential lesson is to be real.

**Design decisions:** a `device_tokens` table with an upsert path and a reaper (tokens rotate on
reinstall, restore, or at FCM's discretion); **dead-token handling** (FCM tells you a token is
`UNREGISTERED` — delete the row, or you accumulate garbage and waste every send; the one piece
easy to skip and shouldn't be); HTTP client (**raw libcurl recommended precisely because** it
forces `std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>` — a custom deleter, new ground for
a codebase whose RAII has all been library-provided); concurrency of sends (**now** the
producer/consumer queue is earned — outbound HTTPS is ~100 ms — but make it conditional on a
measurement); and **where Strategy lands** (two real channels now exist: extract the interface
here).

**Credential handling, made real.** The key must not be in the repo *and* must not be baked into
the image — two different mistakes with two different fixes. Choose between the whole JSON in an
env var (twelve-factor-pure, awkward with multi-line PEM), a file at `0600` with only its *path*
in env (simple, real, **recommended**), or systemd credentials (most correct, most to learn).
**Make it enforceable rather than aspirational: CI fails if a key file is ever committed** — a
`gitleaks` step, or a grep for `BEGIN PRIVATE KEY`. A rule a machine checks is a rule; a rule in
a doc is a hope.

**Add one line under §7: rung 2 is a prerequisite of Phase 4, not an optional parallel track.**
That single edit converts a vague roadmap claim into a scheduling constraint.

---

## The "not yet" register

✅ = already a recorded, deliberate deferral. ⚠️ = nobody decided; registering it here *is* the
decision. **A trigger must be observable** — never "when it feels right".

| Item | Why deferred | Trigger that promotes it |
|---|---|---|
| ✅ **Auth** | One user, LAN-only. §6 non-goal. | A second person needs their own list — **or** the server gets a public IP (rung 2). Rung 2 without auth means anyone who port-scans can POST. |
| ✅ **TLS** | LAN-only. | Rung 2. Then it comes from rung 3's Caddy/nginx, which auto-issues — **you will never write TLS code in C++**, and that is the correct outcome. |
| ✅ **Rate limiting** | One client, human-paced. | Any endpoint reachable without auth from outside the LAN, or B4's harness knocks the server over at a rate one phone could produce. |
| ⚠️ **CORS** | No browser client; Flutter's `http` isn't subject to CORS at all. | You run `flutter build web`, or open a debug HTML page. Not one moment before. |
| ✅ **Prometheus / tracing** | One process, one box. | You cannot answer "why was *that* request slow" from logs in five minutes — concretely, a post-pool p99 regression the harness reproduces and the logs can't explain. |
| ⚠️ **Structured logging + request IDs** | One request at a time is followable. | The first interleaved log from ≥2 concurrent requests you cannot untangle — i.e. **B9**. Most likely entry here to fire first. |
| ✅ **Pagination** | Deferred since Phase 0; B3's `LIMIT` removes the unbounded-scan hazard without it. | The table routinely exceeds the limit **and** you notice rows missing. **Prerequisite:** the timestamp-precision fix below. |
| ⚠️ **Microsecond timestamp precision** | Truncated twice (`to_char`, then `floor<seconds>`); the only consumer renders whole seconds. | Pagination's trigger fires (hard prerequisite), **or** two notifications in the same second must order by time rather than falling through to `id DESC`. |
| ⚠️ **Idempotency keys** | The only writer is a button in your own app. | You add automatic retry to the Dart client, **or** Phase 3's at-least-once loop re-runs a delivery with a user-visible side effect. |
| ⚠️ **Request body cap at the edge** | **Crow cannot do this — verified.** `parser.h:88-91` buffers unbounded; both user extension points (handlers, middleware) run only at message-complete (`parser.h:161-164` → `http_connection.h:147`). `Connection` is an internal type with no substitution hook, so a `before_handle` check would return a correct 413 having already absorbed the payload. C5 is the enforceable half. | **Rung 3.** `client_max_body_size` / `request_body max_size`, one config line, same day as TLS. |
| ⚠️ **`Content-Type` → 415** | Your one client sends it, and enforcing it breaks `curl -d` debugging (defaults to form-urlencoded). Net negative today. | A client you did not compile talks to the API — same trigger as auth. |
| ⚠️ **`GET /v1/notifications/{id}` + `Location` on 201** | The list returns everything. A `Location` pointing at a 404 is worse than no header, so these promote together. | The app gains a detail screen, **or** Phase 4's push payload needs a deep link. *(Honest counter: this is the cheapest excuse to learn Crow's `<int>` URL params. Take it as a lesson if you want — just don't tell yourself it was needed.)* |
| ⚠️ **OpenAPI / contract doc** | A hand-written spec beside a hand-written handler rots; this repo has four measured instances of exactly that. | You want the contract **executable** — B1/B2's functional tier gives you that and cannot rot. Promote the written spec only when a client you don't write needs it. |
| ✅ **Deploy rung 2 (VPS)** | Rung 1 works; the phone is on the LAN. | You want notifications when you're not at home. **Hard prerequisite of Phase 4's credential lesson.** |
| ✅ **Rung 3 (reverse proxy / TLS)** | Nothing to terminate yet. | Immediately on completing rung 2 — a public plaintext port is itself the trigger. |
| ✅ **Rung 4 (managed Postgres)** | Compose Postgres on a volume is fine; no data you'd cry over. | You *would* cry. **Insert a cheaper half-rung first:** `pg_dump` on cron to object storage, before paying $15/mo. |
| ✅ **Rung 5 (k8s)** | One small VPS handles orders of magnitude more than this will see. | Honestly: **probably never**, and saying so is the right answer. If the motive is CV rather than need, do it as a throwaway repo — don't distort this one. |
| ⚠️ **Prepared-statement caching** | Saves microseconds of parse/plan against a millisecond handshake — **and is impossible today**: prepared statements are per-connection, so connection-per-request re-prepares every time. | The pool exists (hard prerequisite) **and** B4/B10 attributes >15% of p99 to parse/plan in `pg_stat_statements`. |
| ⚠️ **Read-through cache** | No read pressure. Buys you the hardest bug class in the industry to solve a problem you can't measure. | B4 shows reads exceeding writes >10:1 **and** GET p99 is DB-bound rather than framework-bound. |
| ⚠️ **`std::shared_mutex`** | §6 lists it for Phase 2, but **a pool is exclusive checkout — `std::mutex` is correct and `shared_mutex` would be wrong there.** | Same trigger as the read-through cache, its only honest home. **Do not bolt it onto the pool to tick a box** — delete it from §6's Phase 2 row and move it to a conditional P8. |
| ⚠️ **Async handlers** | Connection-per-request is synchronous by construction. | B10's pre-registered rule fires. Realistically also requires replacing libpqxx, so the real trigger is "the driver is the bottleneck and you're willing to change drivers". |

---

## Appendix — provenance and caveats

**Verified directly against source during planning:** `backend/include/` has never existed
(`git log --all` empty) despite two skills referencing it; nothing in `.github/` sets
`DATABASE_URL` despite `migrate.sh:6-8` claiming CI does; warning flags absent from both test
targets while sanitizer flags are correctly present; Crow's `port()`/`run_async()`/`stop()`/
`wait_for_server_start()` signatures and the `catch_all` mechanism at `routing.h:1721`/`:1729`;
`const_str`'s array-reference constructor at `utility.h:53-59` and `CROW_MSVC_WORKAROUND`'s
`_MSC_VER < 1900` guard; the unbounded body buffer at `parser.h:88-91`; `docker.md`'s byte
identity; §10 at 68 rows / 56% of the document with two ordering violations.

**Corrections made during review** — four agent claims were wrong and were fixed before reaching
this document: a non-existent `kSelectAll` constant in a definition-of-done; a manufactured
`find` precedence bug in the lint-job prompt; `handle_header()` described as private when it is
public (the conclusion survived on a better argument); and a claim that `CROW_ROUTE` requires a
string literal, when the real constraint is an array with a deducible bound — now written as an
experiment rather than an assertion.

**Not verified — treat with care.** Nothing was built, run or benchmarked; every DoD is a
checkable condition, not a verified one. Specifically: whether `builtin-baseline` resolves from
CI's `--depth 1` vcpkg clone (A1's DoD makes "CI passes" the gate for this reason); which
permission-syntax form this Claude Code version honours (D1); whether `$CLAUDE_PROJECT_DIR` is
exported in hook contexts (D1's echo hook is the probe); TSan's noise level against Crow and asio
(B5 may be an evening of suppressions rather than an hour); `cpp-httplib`'s exact vcpkg port name
(B1); and whether a load tool is available in Ubuntu 24.04's apt (B4). Effort estimates are
judgement calls calibrated to solo evening sessions, not measurements.

**A note on where this file lives.** You asked for it beside `PROJECT_ANALYSIS.md` at the repo
root. Your `organize-docs` routing would put a phase plan in `Documentation/phase-prompts/`.
Moving it is one `git mv` whenever you prefer.
