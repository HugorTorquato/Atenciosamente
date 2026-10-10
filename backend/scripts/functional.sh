#!/usr/bin/env bash
#
# Runs the Python functional (black-box HTTP) test tier end to end.
#
# Why this script exists: unlike the Catch2 tiers, these tests are a *separate
# OS process* that talks to the real atenciosamente_server over a real socket.
# So something has to start a server, wait until it answers, run pytest against
# it, and stop it again. That is all this script is — the manual dance from
# PHASE_3_FUNCTIONAL_END_TO_END_TESTS.md §7.3, in one command.
#
# What it runs, in order:
#   1. ensure the pytest venv exists (creates it + pip installs on first run)
#   2. refuse to start if something is already serving :8080
#   3. apply migrations (idempotent — a no-op unless the schema changed)
#   4. start atenciosamente_server in the background, logging to build/$PRESET/
#   5. poll GET / until it answers — and fail fast if the server died instead
#   6. run pytest
#   7. stop the server (always, via trap) and show its log if anything failed
#
# ⚠️ DESTRUCTIVE: the suite TRUNCATEs the notifications table before EVERY
# test, in the database POSTGRES_DB names — your dev database. Dev data does
# not survive a run. That is deliberate, not a bug: see PROJECT_PLAN.md §10 and
# the TODO(data-management) comment in tests/functional/conftest.py.
#
# ⚠️ Do not run this while a dev server is up, or while `ctest --preset=dev` is
# running — the TRUNCATE would yank rows out from under them. Step 2 catches the
# stray-server case for you; the ctest case is on you.
#
# Usage (inside the dev container: docker compose exec backend bash):
#   scripts/functional.sh                  # the whole suite
#   scripts/functional.sh -- -k smoke      # everything after -- goes to pytest
#   scripts/functional.sh -- -vv -x        # verbose, stop at first failure
#   scripts/functional.sh --preset ci      # use build/ci/ instead of build/dev/
#
# Needs an already-built binary; run `scripts/dev.sh build` first if it's
# missing. This script deliberately does not build, so it stays fast and never
# surprises you with a 20-minute compile.
#
# TODO(ci): nothing calls this automatically yet — CI is deferred, not rejected
# (PROJECT_PLAN.md §8's `functional` job is knowingly unbuilt). It is written to
# stay CI-callable with no changes: --preset ci, pass-through pytest args, and a
# non-zero exit on any failure. Wiring steps are in
# PHASE_3_FUNCTIONAL_END_TO_END_TESTS.md §S2.
set -euo pipefail
cd "$(dirname "$0")/.."

PRESET="dev"
PYTEST_ARGS=()

usage() {
    echo "Usage: scripts/functional.sh [--preset dev|ci] [-- <extra pytest args>]"
    echo "  --preset   which build/ dir to take the server binary from (default: dev)"
    echo "  --         everything after this is forwarded to pytest, e.g. -- -k smoke"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --preset)
            PRESET="$2"
            shift 2
            ;;
        --)
            shift
            PYTEST_ARGS=("$@")
            break
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage
            exit 1
            ;;
    esac
done

SERVER="./build/$PRESET/atenciosamente_server"
VENV="tests/functional/.venv"
LOG="build/$PRESET/functional-server.log"
BASE_URL="${ATENCIOSAMENTE_BASE_URL:-http://localhost:8080}"

# Same guard shape as migrate.sh/dev.sh: this only works inside the container,
# where the toolchain and the POSTGRES_* vars from docker-compose both exist.
if [[ ! -x "$SERVER" ]]; then
    echo "$SERVER not found — build it first:" >&2
    echo "  docker compose exec backend bash" >&2
    echo "  scripts/dev.sh --preset $PRESET build" >&2
    exit 1
fi

# ── 1. Python venv ───────────────────────────────────────────────────────────
# Ubuntu 24.04's system Python is externally managed (PEP 668), so a venv is the
# only sanctioned place to install into. Created once; reused after that.
# Note: a venv built in here records container paths, so it is not usable from
# the WSL host — make a separate one there if you want that workflow.
if [[ ! -x "$VENV/bin/pytest" ]]; then
    echo "==> Creating venv ($VENV)"
    python3 -m venv "$VENV"
    "$VENV/bin/pip" install --quiet --upgrade pip
    "$VENV/bin/pip" install --quiet -r tests/functional/requirements.txt
fi

# ── 2. One server, one database ──────────────────────────────────────────────
# The suite truncates the table a dev server would be serving, so they must not
# run at once. Checked up front rather than letting the bind fail obscurely.
if curl -sf -m 2 "$BASE_URL/" >/dev/null 2>&1; then
    echo "Something is already answering at $BASE_URL — stop it first." >&2
    echo "This suite TRUNCATEs the notifications table that server is serving." >&2
    exit 1
fi

# ── 3. Migrate ───────────────────────────────────────────────────────────────
scripts/migrate.sh

# ── 4. Start the server ──────────────────────────────────────────────────────
# POSTGRES_POOL_SIZE=2 is deliberately smaller than the thread count the
# concurrency test will use, so pool contention is guaranteed rather than lucky.
mkdir -p "$(dirname "$LOG")"
echo "==> Starting server ($PRESET, pool size 2) — log: $LOG"
POSTGRES_POOL_SIZE=2 "$SERVER" > "$LOG" 2>&1 &
SERVER_PID=$!

# trap ... EXIT is RAII for shell scripts: this runs however we leave — success,
# a failed test, set -e aborting, or Ctrl-C — so the server is never orphaned.
# SIGTERM (not SIGKILL) because Crow handles it: app.run() returns normally and
# the ConnectionPool destructor closes its connections.
cleanup() {
    if kill -0 "$SERVER_PID" 2>/dev/null; then
        echo "==> Stopping server (pid $SERVER_PID)"
        kill -TERM "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

# ── 5. Wait for readiness, not just liveness ─────────────────────────────────
# Backgrounding the server returns immediately — before Crow has bound the port
# or the pool has opened its connections. So poll until it actually answers, and
# check it hasn't died, so a startup crash fails fast with the log instead of
# hanging for the whole timeout.
echo "==> Waiting for $BASE_URL"
for _ in $(seq 1 100); do
    if curl -sf -m 1 "$BASE_URL/" >/dev/null 2>&1; then
        READY=1
        break
    fi
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        echo "Server exited during startup. Its log:" >&2
        cat "$LOG" >&2
        exit 1
    fi
    sleep 0.2
done

if [[ -z "${READY:-}" ]]; then
    echo "Server never answered $BASE_URL within 20s. Its log:" >&2
    cat "$LOG" >&2
    exit 1
fi

# ── 6. Run the tests ─────────────────────────────────────────────────────────
echo "==> Running pytest"
STATUS=0
"$VENV/bin/pytest" tests/functional "${PYTEST_ARGS[@]}" || STATUS=$?

# ── 7. Sanitizer reports from the *server* process ───────────────────────────
# pytest is a separate process, so nothing it asserts can see these — a green
# suite can still hide an ASan/UBSan report in the server's log. LeakSanitizer
# is excluded: Crow and libpq hold allocations at exit that aren't our bugs.
if grep -qE "ERROR: AddressSanitizer|runtime error:" "$LOG"; then
    echo "==> Sanitizer report in the server log ($LOG):" >&2
    grep -nE "ERROR: AddressSanitizer|runtime error:" "$LOG" >&2
    STATUS=1
fi

if [[ "$STATUS" -ne 0 ]]; then
    echo "==> FAILED (server log: $LOG)" >&2
else
    echo "==> Functional suite passed"
fi

exit "$STATUS"
