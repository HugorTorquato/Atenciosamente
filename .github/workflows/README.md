# CI pipelines

Two independent GitHub Actions workflows, one per stack. Each has a `paths:` filter so a
mobile-only commit never triggers the backend pipeline and vice versa.

> **Keep this in sync.** Whenever `backend-ci.yml` or `mobile-ci.yml` changes shape (a new
> job, a new step, a different trigger), update the matching diagram below in the same
> commit. See the `backend` / `frontend` subagent instructions, which carry this as a
> standing rule.

---

## Backend — `backend-ci.yml`

Triggers on `push`/`pull_request` to `main` when `backend/**` or the workflow file itself
changes. Two independent jobs run in parallel (no `needs:` between them — see the "Job split
rationale" comment in the workflow file for why) so a broken integration test can't hide a
real unit-test failure, and vice versa.

```mermaid
flowchart TD
    trig(["push / PR → main<br/>(backend/** changed)"]) --> unit
    trig --> integ

    subgraph unit["Job: unit — “Unit tests”"]
        direction TB
        u1[Checkout] --> u2[Install build tools]
        u2 --> u3[Install vcpkg]
        u3 --> u4["Configure + Build<br/>cmake --preset ci"]
        u4 --> u5["ctest -R '^unit/'<br/>no database needed"]
        u5 --> u6["Upload test-results-unit.xml"]
    end

    subgraph integ["Job: integration — “Integration tests (Postgres)”"]
        direction TB
        svc[("services: db<br/>postgres:16<br/>health-checked via pg_isready")]
        i1[Checkout] --> i2["Install build tools<br/>+ postgresql-client"]
        i2 --> i3[Install vcpkg]
        i3 --> i4["Configure + Build<br/>cmake --preset ci"]
        i4 --> i5["Apply migrations<br/>scripts/migrate.sh"]
        i5 --> i6["ctest -R '^integration/'"]
        i6 --> i7["Upload test-results-integration.xml"]
        svc -. reachable at localhost:5432 .-> i5
        svc -. reachable at localhost:5432 .-> i6
    end
```

Both jobs build the same two Catch2 binaries (`tests_unit`, `tests_integration`); what
differs is which one's tests each job runs. `backend/tests/CMakeLists.txt` tags every CTest
name with a prefix per binary, so a single `-R` pattern selects one tier without touching the
other:

| Job | Filter | Covers |
|---|---|---|
| `unit` | `ctest -R '^unit/'` | `src/domain/` — pure logic, no I/O |
| `integration` | `ctest -R '^integration/'` | `src/repository/` — real SQL against the `services: db` sidecar |

Both jobs share the same vcpkg binary cache (`VCPKG_BINARY_SOURCES`) — building twice costs
seconds once warm, not the ~3 minute cold build. `handlers/`/`app.cpp` have no CI tier yet
(functional/E2E tests are Phase 2+, `PROJECT_PLAN.md` §8).

---

## Mobile — `mobile-ci.yml`

Triggers on `push`/`pull_request` to `main` when `mobile/**` or the workflow file itself
changes. A single job: no database, no server — just static analysis and a from-scratch
build to prove the app compiles on a clean machine.

```mermaid
flowchart TD
    trig(["push / PR → main<br/>(mobile/** changed)"]) --> job

    subgraph job["Job: analyze-and-build — “Analyze and build (debug APK)”"]
        direction TB
        m1[Checkout] --> m2["Set up Java 17 (Temurin)"]
        m2 --> m3["Set up Flutter (stable, cached)"]
        m3 --> m4["Accept Android SDK licenses"]
        m4 --> m5["flutter pub get"]
        m5 --> m6["flutter analyze"]
        m6 --> m7["flutter build apk --debug"]
    end
```

`flutter analyze` is the fast, valuable check — type errors, null-safety violations, unused
imports, deprecated API usage, all without a device or emulator. `flutter build apk --debug`
catches what analysis can't (Gradle misconfiguration, missing manifest entries, broken native
plugin bindings) by actually compiling the app end-to-end. `flutter test` isn't run in CI
yet — `mobile/atenciosamente_app/test/widget_test.dart` is currently just a placeholder; add
a test step here once real widget tests exist.
