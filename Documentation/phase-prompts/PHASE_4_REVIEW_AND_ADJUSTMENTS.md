# Phase 4 — Users & Access Control: Review & Adjustments

> An engineering review of
> [`PHASE_4_USERS_AND_ACCESS_ARCHITECTURE.md`](PHASE_4_USERS_AND_ACCESS_ARCHITECTURE.md)
> and
> [`PHASE_4_USERS_AND_ACCESS_IMPLEMENTATION.md`](PHASE_4_USERS_AND_ACCESS_IMPLEMENTATION.md),
> written against the code as it exists today. **Read this after the
> architecture doc.** Where the two disagree, this one is the later thought.
>
> The broad direction is not relitigated here: Argon2id, DB-backed opaque
> tokens, a `role` column on `users`, public self-registration and
> resource-shaped paths are settled owner decisions. Everything below is
> about mechanism, layering, testability and sequencing.
>
> Claims marked **[verified]** were checked against the installed
> dependencies or the running `db` service on 2026-10-09. Claims marked
> **[unverified]** could not be checked yet and must be confirmed during
> implementation.

---

## 1. Verdict

**Sound, keep as written:**

- **D1/D2/D3** — Argon2id for passwords, 256-bit opaque token + fast
  unsalted SHA-256 for sessions, revocation by row delete. The asymmetry
  argument in §4 D3 is correct and well explained; the "salting the token
  would break the indexed lookup" point is exactly right.
- **D5** — `role TEXT NOT NULL CHECK (...)` over a Postgres `ENUM`. The
  transactional-DDL argument is real and it matters to `migrate.sh`, which
  wraps every file in `BEGIN … COMMIT` (`backend/scripts/migrate.sh:38-43`).
- **D6** — never parsing a `role` field. "No code path to get it wrong"
  beats "validated correctly" and is the single best decision in the doc.
- **D7/D8** — email-targeted creation, and two named repository functions
  instead of a flag. D8 in particular keeps authorization out of the
  persistence layer, which is the right boundary.
- **D10/D11** — explicit session parameter on the Dart side, secure storage
  for the token.
- The **`User` / `UserSummary` split** (§7). Making a leak inexpressible
  rather than forbidden is the strongest structural idea in the phase.
- **§8's honesty** about plaintext HTTP, missing rate limiting, and the
  deliberate asymmetry between login's generic `401` and registration's
  informative `409`. Nothing to add; it is a better security section than
  most production projects have.

**The direction is right; the mechanism needs work in four places:** the
authentication guard is in a layer no test target can reach (F1); two
independent `std::once_flag`s will race on `sodium_init()` (F2); the
duplicate-email question has a wrong answer among the two offered (F3); and
the phase's actual security behaviour (401/403/409, the role branch) has no
automated coverage at all in S1–S11 (F4). Everything else is refinement.

Of the eight things you flagged as suspect: **two are confirmed as serious**
(testability of the guard, endpoint coverage), **one is sharpened and
downgraded** (the pool/hashing interaction is a latency and layering problem,
not the pool-starvation problem you expected — see F5), **one is confirmed
and specified** (vague types, F7), **one gets a concrete pick** (the
duplicate race, F3), **one gets a materially better option** (the destructive
migration, F6), **step ordering has one real churn problem and one missing
file** (F13, F14), and on the factual checks **your API claims hold up** —
including the vcpkg names you marked uncertain — with two exceptions noted in
§3. Six findings are things not on your list: F2, F8 (`vcpkg.json` has no
baseline), F9 (the dummy hash as a literal), F10 (`std::tolower` UB), F11
(unbounded password length), F16 (the index-less foreign keys).

---

## 2. Findings

Severity: **[BLOCKER]** = do not implement the phase as written.
**[SHOULD-FIX]** = will cost real time or correctness later.
**[NICE]** = worth a line of code or a sentence.

---

### F1 [BLOCKER] — `authenticate()` lives where no test target can link it

**Amends:** D4, §7, S5.

**What's wrong.** D4 argues for an explicit free function and lists
testability as reason 3: *"`authenticate(txn, req)` is callable from an
integration test with a hand-built `crow::request`, no server and no
middleware chain."* That is true of Crow's API but false of this build:

- `handlers/notifications.cpp` is a source of the **executable**
  `atenciosamente_server`, not of the library (`backend/CMakeLists.txt:66-70`).
  S5 puts `handlers/auth.cpp` on the same target by design
  (implementation doc S4, "note `auth.cpp` and `http_errors.cpp` go on
  `atenciosamente_server`").
- `tests_unit` links `atenciosamente_core` + Catch2 (`backend/tests/CMakeLists.txt:10-13`);
  `tests_integration` links `atenciosamente_core` + libpqxx + Catch2
  (`:55-59`). **Neither links Crow, and neither can see any object file from
  `handlers/`.**

So the most security-critical function in the phase would be reachable only
from `main()`. Concretely: the file that decides whether a request is
authenticated would have **zero** automated tests, and a regression in it
(forgetting to hash the presented token before lookup; dropping the
`expires_at > now()` filter; accepting `Authorization: bearer<token>` without
a space) would be caught by nothing.

**The adjustment.** Split the guard into a Crow-free core and a one-line
Crow adapter, and put the core in a new `src/auth/` layer that compiles into
`atenciosamente_core`.

```cpp
// src/domain/bearer_token.hpp — pure, unit-testable, includes only
// <optional>/<string>/<string_view>
//
// Returns the token from an "Authorization: Bearer <token>" header value, or
// nullopt if the header is absent/empty, does not start with a
// case-insensitive "Bearer" followed by whitespace, or carries an empty token.
std::optional<std::string> extract_bearer_token(std::string_view authorization_header);
```

```cpp
// src/auth/authenticator.hpp — needs pqxx, must not include Crow.
// Compiled into atenciosamente_core, so tests_integration can call it.
std::optional<AuthenticatedUser> authenticate_bearer(pqxx::work& txn,
                                                     std::string_view authorization_header);
```

```cpp
// src/handlers/auth.hpp — the Crow seam, deliberately too small to need a test
inline std::optional<AuthenticatedUser> authenticate(pqxx::work& txn,
                                                     const crow::request& req) {
    return authenticate_bearer(txn, req.get_header_value("Authorization"));
}
```

`crow::request::get_header_value` returns a reference to an empty string when
the header is absent (`crow/http_request.h:81-84`, `crow/http_request.h:33-43`)
**[verified]**, so the adapter needs no null handling and `authenticate_bearer`
receives `""` for "no header" — which must map to `nullopt`.

**Why a new `src/auth/` folder rather than `repository/` or `domain/`:**
`authenticate_bearer` takes a `pqxx::work&`, so it cannot be `domain/`; it
contains no SQL and makes a policy decision, so it should not be
`repository/`. Every existing `src/` subfolder names a layer (§10, 2026-07-27
entry) and this is a genuinely new layer: *"policy that needs the database but
not HTTP."* That is also exactly where Phase 4's "whose notification is due?"
logic will want to live. See §6 open question 1 if you would rather not add a
fifth folder.

**What this buys in tests:** `extract_bearer_token` gets unit tests (no header,
empty header, `"Bearer"` with no token, `"bearer abc"`, `"Basic abc"`,
leading/trailing spaces, a token containing a space). `authenticate_bearer`
gets integration tests against real rows: valid token resolves to the right
user and role; an expired row resolves to `nullopt`; a deleted row resolves to
`nullopt`; a syntactically valid but unknown token resolves to `nullopt`; and —
the one that matters most — **a raw token stored verbatim in `token_hash` does
not authenticate**, which is the regression test for "did we remember to hash".

---

### F2 [BLOCKER] — two `std::once_flag`s will race on `sodium_init()`

**Amends:** §7 (third structural bullet), S1, S2.

**What's wrong.** §7 says *"`password.cpp` guards it with a `std::once_flag`
internally. Callers can't get the ordering wrong because there's no ordering
to get wrong."* S1's prompt repeats it. S2 then adds `domain/token.cpp`, which
also calls libsodium (`randombytes_buf`, `crypto_hash_sha256`) and therefore
also needs initialization — and if it copies the S1 pattern it gets its **own**
`std::once_flag` in its own anonymous namespace.

Two independent once-flags do not serialize against each other.
`std::call_once` guarantees "exactly once per flag", not "exactly once per
program". A first login (thread A, `password.cpp`'s flag) concurrent with a
first authenticated `GET` (thread B, `token.cpp`'s flag) produces **two
concurrent `sodium_init()` calls** — precisely the thing libsodium documents as
unsafe, and precisely the hazard §7 claims to have designed away. It will not
reproduce reliably, ASan and UBSan will not flag it, and it only happens on the
first two requests after a restart, which is the worst possible failure profile.

**The adjustment.** One translation unit owns initialization; both callers go
through it.

```cpp
// src/domain/sodium.hpp
// Initializes libsodium exactly once per process, no matter how many
// callers or threads reach here first. Safe to call from any thread and
// from every entry point that touches libsodium (hashing, RNG, SHA-256) —
// there is no ordering for a caller to get wrong, and no init call for
// main() or Catch2's main() to remember.
//
// Throws std::runtime_error if sodium_init() reports failure (return < 0).
// Deliberately does not treat the "already initialized" return (1) as an
// error.
void ensure_sodium_initialized();
```

```cpp
// src/domain/sodium.cpp
namespace { std::once_flag g_sodium_once; }

void ensure_sodium_initialized() {
    std::call_once(g_sodium_once, [] {
        if (sodium_init() < 0) {
            throw std::runtime_error("ensure_sodium_initialized: sodium_init() failed");
        }
    });
}
```

`std::call_once` leaves the flag unset if the callable throws, so a later
retry re-runs the init rather than silently proceeding with an uninitialized
library — which is the behaviour you want here. Add `domain/sodium.cpp` in
**S1** and have `password.cpp` and `token.cpp` both call
`ensure_sodium_initialized()` as their first statement.

---

### F3 [BLOCKER] — the duplicate-email race: catch the violation, and know that it poisons the transaction

**Amends:** S4 ("either check `find_by_email` first or catch the unique
violation — pick one").

**Pick: catch the violation. Do not pre-check.**

**Why the pre-check is the wrong answer.** It is a textbook
time-of-check/time-of-use gap. Two simultaneous `POST /users` for
`a@b.c`: both transactions run `SELECT … WHERE email = 'a@b.c'`, both see no
row (neither can see the other's uncommitted insert — this is `READ
COMMITTED`, the pqxx default), both insert, the first commits, the second gets
a `23505` from the unique index. If the only handling is the pre-check, that
second request becomes a **500**, which is the exact bug S4 says to avoid, and
it is unreproducible by hand. The `UNIQUE` index is the only thing in the
system that actually serializes the two writers, so it has to be the thing you
react to. A pre-check *in addition* costs a round trip on every single
registration and still leaves you needing the catch — so it buys nothing.

**Exact mechanics.**

```cpp
#include <pqxx/except>   // brings in the typed exception hierarchy

try {
    created = user_repository::insert(txn, request->email, password_hash);
} catch (const pqxx::unique_violation&) {
    return conflict("email already registered");   // 409
}
```

- `pqxx::unique_violation` exists in the pinned libpqxx (8.0.2) and derives
  `integrity_constraint_violation` -> `sql_error` -> `failure` ->
  `std::exception` (`pqxx/except.hxx:766`, `:714`, `:381`, `:75`) **[verified]**.
- **Catch the precise type.** `catch (const pqxx::sql_error&)` would turn a
  syntax error into a 409; `catch (const std::exception&)` would turn a dropped
  connection into a 409.
- **Scope the `try` to the user insert only.** In the merged register flow the
  same transaction also inserts a session row, and `sessions.token_hash` is
  also `UNIQUE` — a wider `try` would report a token collision as
  "email already registered".
- **The transaction is dead after the throw.** `pqxx::sql_error` overrides
  `poisons_transaction()` to return `true` (`pqxx/except.hxx:399-403`)
  **[verified]**: Postgres puts the session in failed-transaction state and no
  further statement can run on that `pqxx::work`. So the handler must return
  immediately and let `~work()` issue the `ROLLBACK` — it must **not** try to
  do anything else in that transaction. If you ever need to continue, the tool
  is `pqxx::subtransaction` (a `SAVEPOINT`), which exists in this version
  (`pqxx/subtransaction.hxx:80`) **[verified]** — but you don't need it here.
- **Where the catch goes: the handler, not the repository.** The repository
  exposes a capability; mapping a constraint violation to an HTTP status is a
  decision, and decisions live in `handlers/` (same reasoning as D8).

**Test it.** The integration test S3 already plans ("a second insert of the
same email throws") should assert the *typed* exception, not just any throw:
`REQUIRE_THROWS_AS(user_repository::insert(txn, email, hash), pqxx::unique_violation);`
That is the assertion the handler's `catch` clause depends on.

---

### F4 [BLOCKER] — the phase's security behaviour is verified only by curl

**Amends:** §1's definition of done, S4/S5/S6's "Manual check" bullets,
`PROJECT_PLAN.md` §8.

**What's wrong.** Walk the planned suite against what the phase actually does:

| Behaviour | Where it lives | Planned coverage |
|---|---|---|
| Argon2id round trip | `domain/password.cpp` | unit (S1) |
| token shape, hash determinism | `domain/token.cpp` | unit (S2) |
| body validation, lowercasing, min length | `domain/*_request.cpp` | unit (S4, S5) |
| user/session row round trips, expiry filtering | `repository/` | integration (S3, S5) |
| `get_all_for_user` does not leak other users' rows | `repository/` | integration (S6) |
| **401 on no / bad / expired token** | `handlers/` | **curl, once** |
| **403 for a recipient on POST** | `handlers/` | **curl, once** |
| **the GET role branch** (`get_all` vs `get_all_for_user`) | `handlers/` | **curl, once** |
| **409 on duplicate email** | `handlers/` | **curl, once** |
| **the dummy-hash timing equalizer runs on the unknown-email path** | `handlers/` | **nothing** |
| **`role` in the request body is ignored** | `handlers/` + domain | **partially** |
| **`WWW-Authenticate: Bearer` on 401** | `handlers/` | **nothing** |

Every row in the top half is the easy half. Every row in the bottom half is
the phase's actual reason to exist, and it is exactly the half that
`PROJECT_PLAN.md` §8 has been deferring to "Phase 2+" since Phase 0. Phase 2
shipped without it.

**The concrete failure scenario.** In S6, `handlers/notifications.cpp` gains:

```cpp
if (user->role != "admin") return forbidden();
```

Invert that comparison — one character — and every recipient can create
notifications for anyone while every admin is locked out. The *entire* planned
suite stays green: `domain/` is untouched, both repository functions still
behave correctly, and the integration test proving `get_all_for_user` filters
by user proves nothing about which function the handler called. The
regression would be caught only by someone re-running S6's manual curl script
by hand.

That is not an acceptable bar for the one phase in the roadmap whose subject
is access control. **Phase 4 is where the functional tier earns its place.**

**Recommendation: build it, in-process, with Crow's own dispatcher — not over
a socket.** `crow::SimpleApp` exposes `handle_full(request&, response&)`
(`crow/app.h:284`) and `validate()` (`crow/app.h:589`) **[verified]**, and
`crow::request` is default-constructible with public `method`/`url`/`body`
members plus `add_header()` (`crow/http_request.h:46-79`) **[verified]**. So a
functional test is a Catch2 `TEST_CASE` that builds a request, calls
`handle_full`, and asserts on `res.code` and `res.body` — no listening socket,
no server thread, no port, no `curl`, no flakiness from either.

What it costs, honestly:

1. **A CMake restructure.** `app.cpp` and `handlers/*.cpp` move out of the
   `atenciosamente_server` executable into a new static library
   `atenciosamente_http` (linking `atenciosamente_core` and `Crow::Crow`
   PUBLIC); the executable becomes `main.cpp` alone. This is not a new idea —
   `PROJECT_PLAN.md` §10 (2026-04-25) already decided *"`main.cpp` / `app.cpp`
   split — `app.cpp` can link into a test target without pulling in `main()`"*.
   The target to make that true was simply never built. ~25 lines of CMake.
2. **A fixture.** `setup_routes(crow::SimpleApp&, ConnectionPool&)` already
   takes the pool by reference (`src/app.cpp:5`), so the fixture constructs
   its own `ConnectionPool pool{2}` and its own app. ~60 lines including
   small `get`/`post` helpers that attach `Authorization` and
   `Content-Type`.
3. **A different isolation strategy, and this is the real cost.** Handlers
   call `txn.commit()`. The per-test rollback trick that
   `tests/integration/repository/notification_repository_test.cpp:9-29` is
   built on **cannot work here** — that is the honest reason this is a third
   tier and not more files in `tests_integration`. Use: a random per-test
   email (`"func-" + <hex from randombytes> + "@test.local"`), and a teardown
   that runs `DELETE FROM users WHERE email LIKE 'func-%@test.local'`
   (sessions and notifications follow via `ON DELETE CASCADE`). Scoping the
   cleanup to a prefix rather than `TRUNCATE` matters: it must not delete the
   admin account you created by hand per D6.
4. **One CI job** (~40 lines, copy the `integration` job and change the
   `ctest -R` filter to `'^functional/'`) **plus the backend diagram in
   `.github/workflows/README.md`, in the same commit.**
5. **Two extra conversations** in the step list (see §5: new S6 and the
   functional assertions folded into S7/S8).

**If you decline the tier, do this instead** — it is cheap, it is strictly
better design regardless, and I recommend doing it *even if you build the
tier*: lift the authorization decision out of the Crow adapter into a pure
function.

```cpp
// src/domain/access.hpp
enum class NotificationScope { OwnOnly, All };

// What a caller is allowed to read. Admins see every notification;
// recipients see only their own.
NotificationScope notification_read_scope(const AuthenticatedUser& user);

// Whether a caller may create notifications for other people (admin only).
bool may_create_notifications(const AuthenticatedUser& user);
```

Those are unit-testable with no database, no Crow and no new target, the
`"admin"` string stops appearing inside a handler, and the handler shrinks to
a dispatch that is genuinely too small to get wrong. It does not cover
401/403/409 status mapping or the `WWW-Authenticate` header — only the
functional tier does that — but it removes the single highest-risk
untested branch for about fifteen lines of code.

---

### F5 [SHOULD-FIX] — hashing inside the lease: your instinct is right, the mechanism is not

**Amends:** D4's code snippet, S4, S5.

You suspected pool starvation. **It is not pool starvation at the default
configuration, and the numbers say so:**

- `main()` calls `app.port(8080).multithreaded().run()` (`src/main.cpp:25`).
  `multithreaded()` is `concurrency(std::thread::hardware_concurrency())`
  (`crow/app.h:447-450`), and Crow runs `concurrency_ - 1` worker io_context
  threads (`crow/http_server.h:130`) **[verified]**.
- The dev container reports `nproc` = 4 **[verified]**, so **3 handler threads**
  against **4 pool slots** (`kDefaultPoolSize = 4`, `src/main.cpp:9`).
  `ConnectionPool::acquire()` can never block in this configuration.
- Memory is a non-issue: 3 concurrent Argon2id at
  `crypto_pwhash_MEMLIMIT_INTERACTIVE` is ~192 MiB; the container has 16 GiB
  with **no cgroup memory limit** (`memory.max` reads
  `9223372036854771712`) and `ulimit -l` is `unlimited` **[verified]**, so
  neither an OOM nor an `mlock` failure is a realistic risk here. Drop that
  worry.

**What the real problems are:**

1. **A blocking handler stalls every other socket on its io_context thread.**
   Crow multiplexes many connections onto each of its 3 worker threads.
   A handler that spends ~60 ms inside Argon2id is not "one slow request" —
   it is one worker thread unavailable to *every* connection assigned to it.
   Three concurrent logins occupy all three workers and the server stops
   answering anything, including `GET /` and `GET /notifications`, for the
   duration. Ten queued logins give a ~0.7 s tail on unrelated endpoints.
   This is the concrete shape of the CPU/DoS vector §8 already names, and
   it is a property of Argon2id, not of the pool.
2. **`POSTGRES_POOL_SIZE` is configurable and nothing enforces the one
   invariant that keeps `acquire()` non-blocking.** Set it to 1 or 2 (easy to
   do while debugging) and `acquire()` starts blocking *inside* an io_context
   thread — a request that has not even been authenticated yet now stalls
   other connections while waiting for a connection slot. Same if the
   process ever runs on a machine with more cores than `POSTGRES_POOL_SIZE`.
3. **Holding a `pqxx::work` open across the hash leaves an `idle in
   transaction` session in Postgres for tens of milliseconds**, holding an
   MVCC snapshot and pinning the slot. Harmless at this scale; free to avoid;
   and it is a habit that gets expensive the moment the hash cost or the
   traffic goes up.

**The adjustment — scope the lease, nothing else changes.** `ConnectionLease`
needs no API change: it releases in its destructor
(`src/db/connection_pool.cpp:38-42`), so a pair of braces is the whole fix.
The existing `handle_post_notification` already sets the precedent —
it parses and validates *before* `pool.acquire()`
(`src/handlers/notifications.cpp:45-56`). Keep doing that.

**Registration** is trivially correct if you just hash first:

```
parse + validate body                      (no lease)
hash_password(...)                         (no lease, ~60 ms)
    acquire -> work -> insert user -> insert session -> commit -> release
build 201
```

**Login needs two short transactions**, and that is fine:

```
parse + validate body                      (no lease)
    acquire -> work -> find_by_email -> release        (read, ~1 ms, no commit)
verify_password(plain, user ? user->password_hash
                            : dummy_password_hash())   (no lease, ~60 ms)
on failure: 401, generic                   (no lease held at any point)
generate_session_token()                   (no lease)
    acquire -> work -> insert session -> commit -> release
build 201
```

Splitting login across two transactions is safe because there is nothing to
be atomic about: reading the user and creating the session do not need a
consistent snapshot of each other. The only interleaving is "the account was
deleted in the ~60 ms while we hashed", which makes the session insert fail
the foreign key — catch `pqxx::foreign_key_violation` and return the same
generic `401`. There is no account-deletion feature yet, so this is
belt-and-braces.

**Authenticated requests keep one lease and one transaction.** The token
lookup is a single indexed read (microseconds), so `authenticate()` and the
notification query should share one `pqxx::work` — exactly as D4's snippet
shows. The lease-scoping rule applies only to the two endpoints that hash.

**Also add the missing invariant.** In `main()`, after reading
`POSTGRES_POOL_SIZE`, log a warning if the pool is smaller than Crow's
concurrency:

```cpp
// A pool smaller than Crow's worker-thread count means acquire() can block
// on an io_context thread, stalling unrelated connections multiplexed onto
// the same thread. Warn rather than throw: it is a tuning mistake, not a
// misconfiguration that makes the server wrong.
```

That turns an invisible latency cliff into a startup line of output.

---

### F6 [SHOULD-FIX] — `0004` can be non-destructive and still end at `NOT NULL`

**Amends:** §5's `0004`, S6.

**What's actually true about the current proposal.** It is not an idempotency
problem — `migrate.sh` guards by filename in `schema_migrations`
(`backend/scripts/migrate.sh:28-44`) and wraps the file in `BEGIN … COMMIT`,
so it runs exactly once and atomically. On a fresh clone the `DELETE` hits an
empty table and does nothing. The whole cost falls on exactly one database:
your existing dev DB. So the honest severity is "annoying once", not "data
loss".

**But there is a materially better version, and it is four lines longer.**
Write the migration as *add nullable -> backfill -> enforce*, which is the
standard shape for exactly this change and happens to be non-destructive
whenever a user exists:

```sql
-- 0004_add_user_id_to_notifications.sql
--
-- Notifications created before Phase 4 have no owning user. Rather than
-- dropping them outright, this adopts them to the oldest account and only
-- deletes the ones that cannot be adopted (i.e. when the database has no
-- users at all, which is the case on a fresh clone where the table is empty
-- anyway, so every statement here is a no-op).
--
-- Ordering note for the existing dev database: register your account via
-- POST /users BEFORE running this migration if you want the old rows kept.
--
-- The column is added NULLABLE and only then made NOT NULL, so each step
-- fails loudly on its own terms instead of the ADD COLUMN failing with
-- "column contains null values" and leaving you to guess why.
ALTER TABLE notifications
    ADD COLUMN IF NOT EXISTS user_id BIGINT REFERENCES users(id) ON DELETE CASCADE;

UPDATE notifications
   SET user_id = (SELECT id FROM users ORDER BY id LIMIT 1)
 WHERE user_id IS NULL;

DELETE FROM notifications
 WHERE user_id IS NULL;

ALTER TABLE notifications
    ALTER COLUMN user_id SET NOT NULL;

-- Postgres does NOT index the referencing side of a foreign key. Both
-- get_all_for_user()'s WHERE and the ON DELETE CASCADE above need this.
CREATE INDEX IF NOT EXISTS notifications_user_id_idx ON notifications (user_id);
```

This keeps the architecture doc's reasoning (a permanently nullable `user_id`
would push a "what does NULL mean here?" branch into every scoped query
forever — correct, and that is why `SET NOT NULL` is still the last step), but
it stops being the one destructive step in the phase, and it is the "real
backfill" §5 says to write if the data ever matters. It also fixes F16 for
`notifications` in the same file.

**Keep the loud warning in S6's prompt anyway** — the `DELETE` is still there
for the no-users case, and surprising someone with it would be worse than the
deletion.

---

### F7 [SHOULD-FIX] — pin down the vague types; delete one of them

**Amends:** §7's file list, S3, S5.

The docs name `domain/session.hpp` without defining it and have
`find_valid_by_token_hash` return *"an `AuthenticatedUser`-ish value"*. Here
is the concrete set. Four decisions are embedded in it.

```cpp
// src/domain/user.hpp
struct User {                   // the users row, as the login path needs it
    std::int64_t id;
    std::string email;
    std::string password_hash;
    std::string role;
};

struct UserSummary {            // the users row, as HTTP is allowed to see it
    std::int64_t id;
    std::string email;
    std::string role;
};
```

```cpp
// src/domain/authenticated_user.hpp
// The verified identity of whoever made the current request. Carries no
// token, no token hash, no password hash and no expiry: it is the answer to
// "who is asking", and nothing downstream of the guard needs the credential
// that produced it.
struct AuthenticatedUser {
    std::int64_t id;
    std::string email;
    std::string role;
};
```

```cpp
// src/domain/auth_response.hpp
// The body POST /users and POST /sessions both return, so register and
// login share one serializer and the client has one code path.
// Deliberately does NOT carry expires_at — see decision 3 below.
struct AuthResponse {
    std::string token;
    UserSummary user;
};
```

**Decision 1: `domain/session.hpp` should not exist.** Nothing on the server
holds a session as a value. `session_repository::insert` needs no return
value, the lookup returns an `AuthenticatedUser`, and the delete returns a
`bool`. The only thing resembling a "session" is the *response body*, which is
`AuthResponse` above with `to_json` in a new
`domain/auth_response_json.{hpp,cpp}`. Remove `session.hpp` from §7's tree. The
mobile `models/session.dart` (token + user) is the right shape on that side and
mirrors `AuthResponse` exactly.

**Decision 2: `AuthenticatedUser` is a distinct struct, not `UserSummary`
renamed**, even though the fields currently coincide. `UserSummary` is a
response DTO and `AuthenticatedUser` is an input to a policy decision; they
will diverge the first time you stop echoing `role` to clients, or add
`last_seen_at` to one and not the other. Naming the type
`AuthenticatedUser` also documents at every call site that authentication
already happened. The cost is one extra five-line header and a one-line
`to_summary()` if a handler ever needs to echo the caller back — which no
endpoint in §6 does. (Flip this if you prefer; see §6 open question 2.)

**Decision 3: no timestamps cross the C++ boundary in this phase.** Neither
`User`, `UserSummary`, `AuthenticatedUser` nor `AuthResponse` carries a
`created_at` or `expires_at`. This matters because of a detail the plan gets
slightly wrong: this project **does** parse timestamps in C++ — see
`parse_created_at` at `src/repository/notification_repository.cpp:17-34`,
which `sscanf`s a Postgres-formatted string into chrono calendar types,
precisely because libstdc++ formats but cannot parse (§10, 2026-07-26). That
function is `static` in an anonymous namespace
(`notification_repository.cpp:12-45`), so S3's instruction to *"read
`created_at` with the same `to_char(...)` expression"* in the **user**
repository would force you to either duplicate it or promote it to a shared
header — new work, to populate a field no endpoint in §6 returns.
`POST /users` returns `{"id","email","role"}`. So: **drop `created_at` from
the user types entirely.** The column stays in the table (it is free and you
will want it later); C++ just never selects it. If a later phase does need it,
the first step is extracting `kCreatedAtSelectExpr` + `parse_created_at` into
a shared `repository/timestamps.{hpp,cpp}` — a worthwhile refactor, but not
Phase 4's job.

**Decision 4: the role strings live in exactly one C++ place.** Right now
`"admin"` and `"recipient"` would appear in the `0002` CHECK constraint, in
`user_repository::insert`, in the notifications handler's role test, and in
the Flutter FAB condition. Add:

```cpp
// src/domain/roles.hpp
// The two values users.role is constrained to by 0002_create_users.sql's
// CHECK. Kept as string_view (not an enum) so the column keeps
// round-tripping as a plain std::string, per D5.
inline constexpr std::string_view kRoleAdmin = "admin";
inline constexpr std::string_view kRoleRecipient = "recipient";
```

and have `domain/access.hpp` (F4) be the only code that compares against them.

**Finally, rename the repository function** so its name matches what it
returns: `session_repository::find_valid_by_token_hash` -> **`find_authenticated_user`**.
"find_valid" returning a user reads like a leftover from an earlier design.

---

### F8 [SHOULD-FIX] — `vcpkg.json` has no `builtin-baseline`, and S1 is the worst time to discover that

**Amends:** S1, S7, and `PROJECT_PLAN.md` §10's 2026-04-23 entry.

`backend/vcpkg.json` contains only `name`, `version-string` and
`dependencies` — **there is no `builtin-baseline` field** and no baseline in
`CMakePresets.json` **[verified]**. But `PROJECT_PLAN.md` §10 (2026-04-23)
asserts *"the authoritative pin is `builtin-baseline` in vcpkg.json"*. The
decision log describes a pin that does not exist.

Why it bites now specifically: CI clones vcpkg at HEAD with `--depth 1`
(`.github/workflows/backend-ci.yml:112`, `:227`), so with no baseline every
cold-cache CI run resolves dependency versions against whatever the vcpkg
registry's tip is that day. Today's container has Crow 1.3.3 and libpqxx
8.0.2 **[verified]**; CI may well have something else. Phase 4 adds
**libsodium 1.0.22-1, which is an autotools port** (`vcpkg-make` host
dependency, `AUTORECONF` in its portfile) **[verified]** — the first
dependency in this project whose build is a `./configure` run rather than a
CMake one. Adding a floating native port to an unpinned manifest, in the step
whose own risk table already says "cold-cache CI build of a new native port
fails or times out", is asking for a CI failure that is impossible to
reproduce locally.

**The adjustment:** in **S1**, before adding libsodium, run
`vcpkg x-update-baseline --add-initial-baseline` in `backend/` (or paste the
current registry commit into a `"builtin-baseline"` field by hand), commit
that as its own change, and correct the §10 entry. Then add libsodium. This is
a five-minute task that converts a class of unreproducible CI failures into a
deliberate, reviewable version bump.

---

### F9 [SHOULD-FIX] — the dummy hash must be computed, not written down

**Amends:** §8's second bullet, S5.

§8 says login's unknown-email path *"still runs `verify_password` against a
fixed dummy Argon2id hash"*. The word "fixed" invites a string literal in the
source — and a literal PHC string carries the cost parameters it was created
with, baked in: `$argon2id$v=19$m=65536,t=2,p=1$…`. The timing equalizer only
equalizes while those parameters match the ones `hash_password` currently
uses. The day you raise the cost from `INTERACTIVE` to `MODERATE` (a one-line
change, with a §4 D1 note saying old hashes keep verifying — true, and that is
the problem), the dummy becomes measurably cheaper than a real verification
and **the enumeration oracle silently reopens**, with no test failing and
nothing in the diff hinting at it.

**The adjustment — derive it from the real function, so it cannot drift:**

```cpp
// src/domain/password.hpp
// A valid Argon2id hash of an arbitrary throwaway password, produced by
// hash_password() itself so it always carries the CURRENT cost parameters.
// verify_password() against this is what the login handler runs when no user
// matches the submitted email, so "unknown email" takes the same measurable
// time as "wrong password" (see the architecture doc §8). Deriving it instead
// of hardcoding a PHC literal is what keeps that true after a future cost
// increase.
//
// Computed once, lazily, on first use: the first failed-login-for-an-unknown-
// email after a restart pays one extra hash, and nothing else ever does.
const std::string& dummy_password_hash();
```

```cpp
// src/domain/password.cpp
const std::string& dummy_password_hash() {
    static const std::string kDummy = hash_password("timing-equalizer-not-a-real-password");
    return kDummy;
}
```

The function-local `static` gets thread-safe initialization from the language
(C++11 magic statics), so no `once_flag` and no `main()` involvement — same
reasoning as F2, and it keeps working under Catch2's `main()`.

**And add the test F4 says is missing:** a functional `TEST_CASE` asserting
that `POST /sessions` with an unknown email and `POST /sessions` with a known
email and a wrong password return byte-identical bodies and the same status.
That does not test the *timing*, which no cheap test can, but it locks the
response shape, which is the half that silently drifts when someone adds a
helpful error message.

---

### F10 [SHOULD-FIX] — `std::tolower(char)` on a UTF-8 email is undefined behaviour

**Amends:** §5's case-handling note, S4, S5.

§5 says emails are lowercased in C++ with "one `std::tolower` pass". Written
the obvious way, that is UB:

```cpp
for (char& c : email) c = std::tolower(c);                  // WRONG
```

`std::tolower` requires its argument to be representable as `unsigned char` or
equal to `EOF`. `char` is signed on x86-64, so any byte above 0x7F — i.e. any
non-ASCII character in an email address, which is legal and which a Brazilian
user is plausibly going to type — arrives as a negative `int` and indexes
glibc's classification table out of bounds. ASan may or may not catch it
depending on where that table sits; UBSan will not.

```cpp
for (char& c : email) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));   // correct
}
```

Worth calling out in S4's prompt as a thing to get right rather than
discover: it is the single most common C++ character-handling bug and this
phase introduces the project's first `tolower` call.

Two related notes while you are in that function:

- Lowercasing only affects ASCII bytes. `std::tolower` will not case-fold
  `Á`; two addresses differing only in the case of a non-ASCII letter remain
  distinct accounts. That is acceptable and matches what mail providers
  mostly do — just do not claim the pass makes the column
  "case-insensitive" in general.
- **Add `CHECK (email = lower(email))` to the `users` table** (see the
  revised `0002` in §4). The C++ side relies on the invariant "every stored
  email is lowercase" for `find_by_email` to work at all; one hand-written
  `INSERT` in `psql` with a capital letter creates a row no login can ever
  find. One CHECK constraint makes the database enforce what the application
  assumes, in exactly the spirit of D5.

---

### F11 [SHOULD-FIX] — "no maximum password length" is wrong as stated

**Amends:** §8's password-rules bullet, S4.

§8 concludes *"No maximum length either — Argon2id has no input cap (unlike
bcrypt, D1)."* The premise is right and the conclusion does not follow.
bcrypt's problem is **silent truncation at 72 bytes** — a correctness bug
because it is invisible. An explicit cap that *rejects* with a 400 is not the
same thing: nothing is silently discarded and the user is told.

Without a cap, `POST /sessions` with a 50 MB `password` field makes the server
buffer 50 MB in `req.body`, parse it as JSON, copy it into a `std::string`,
and feed it to Argon2id — on one of only three io_context threads (F5). It is
a free amplification knob on the endpoint §8 already names as the project's
main DoS surface.

**The adjustment:** cap the password at 1024 bytes and the email at 254 bytes
(the RFC 5321 practical maximum), both as named constants in
`domain/register_user_request.hpp`, both rejected with a 400 and an explicit
message. 1024 is far past any real passphrase and still bounds the work. Keep
the *minimum* at 8 with no composition rules — that part of §8 is correct and
well argued.

Apply the same caps on the **login** validator, not just registration. S5
currently says login skips the minimum-length rule ("that's a registration
rule") — right — but the *maximum* is a resource bound, not a policy, and it
belongs on both.

---

### F12 [SHOULD-FIX] — `make_interval(days => $n)` instead of `$n * INTERVAL '1 day'`

**Amends:** D9, S5.

Your SQL works. Verified against this project's own Postgres 16:

```
PREPARE p1 AS SELECT now() + ($1 * INTERVAL '1 day');
  -> parameter_types = {"double precision"}     EXECUTE p1(30) -> ok
PREPARE p2 AS SELECT now() + make_interval(days => $1);
  -> parameter_types = {integer}                EXECUTE p2(30) -> ok
```

**[verified]** No cast is needed: `*` has no `integer * interval` operator, so
Postgres resolves the untyped parameter to `double precision` via the implicit
`int4 -> float8` cast. pqxx sends parameters as text with an unspecified type
OID, so a C++ `int` of 30 arrives as `"30"` and parses as `30.0`. It is
correct.

It is also a float, which is a slightly odd thing for "days" to be. Prefer:

```sql
INSERT INTO sessions (user_id, token_hash, expires_at)
VALUES ($1, $2, now() + make_interval(days => $3))
```

The parameter types as an integer, which is what the C++ constant is, and the
intent reads off the line without the reader having to know the operator
table. A one-token change for a clearer type signature at the DB boundary.

Where the constant lives: put
`inline constexpr int kSessionTtlDays = 30;` in **`handlers/auth.hpp`**,
alongside the endpoint contract comments. The TTL is observable endpoint
behaviour, so it belongs in the header a reader consults to understand the
endpoint — and D9's "policy in C++, arithmetic in SQL" split is preserved
either way. (Whether it should be an env var instead is §6 open question 3.)

---

### F13 [SHOULD-FIX] — the S4 -> S5 response-shape churn is avoidable; re-cut the DB steps

**Amends:** S4, S5.

The churn is real: S4 ships `POST /users` returning `201 {user}`, then S5
changes the same handler to return `201 {token, user}`. That invalidates S4's
own manual-check instructions, rewrites the handler's header comment and
response construction, and — once the functional tier from F4 exists — makes
S4 write assertions that S5 immediately rewrites. It is maybe twenty minutes
of rework, but it also means the step boundary ships a contract that was never
intended, which is the kind of thing that ends up in a client by accident.

**The cause is that the step order puts the first endpoint before the sessions
table.** Both fixes move work, not features:

- **Option A (recommended):** move the `sessions` migration +
  `session_repository::insert` *earlier*, into their own step before any
  handler exists. Then the first endpoint step ships `{token, user}` directly
  and nothing is rewritten. See §5's S4.
- **Option B:** keep the order and ship `POST /users` returning `201 {user}`
  as a deliberate intermediate contract, documented as such. Cheaper to
  write, but you pay the rework and the stale curl script.

Take A. It also has the side benefit of putting the F1 restructure
(`authenticate_bearer` in core) before any Crow code exists, which is the
natural place for it — you cannot accidentally write the untestable version if
the testable one already exists.

---

### F14 [SHOULD-FIX] — S6's file list is missing a test it will break

**Amends:** S6.

S6 adds `user_id` and `recipient_email` to `Notification` and serializes them.
Its file list names `tests/unit/create_notification_request_test.cpp` and
`tests/integration/repository/notification_repository_test.cpp` — but **not
`tests/unit/notification_json_test.cpp`**, which S6 breaks:

- It brace-initializes `Notification` positionally:
  `{1, "Title", "Body", epoch}` and `{42, "Hello", "World", epoch}`
  (`tests/unit/notification_json_test.cpp:29`, `:44`, `:57-59`). Insert
  `user_id` anywhere before `created_at` in the struct — the natural place —
  and these stop compiling (a `std::string` initializer landing on an
  `int64_t` member). Append the new members at the end and they compile but
  silently leave the new fields zero-initialized.
- Its "single notification has all required keys" section
  (`:27-38`) enumerates the expected keys and will no longer describe the
  serializer's output.

Add it to S6's file list and to the step's "green" criterion. While you are
there, this test is the right place to assert the new keys are present and
that `recipient_email` round-trips — it is the cheapest possible coverage for
the serializer change.

---

### F15 [SHOULD-FIX] — the Flutter app is broken from S6 until S10, and the docs should say so

**Amends:** the implementation doc's "Step order" narrative, §4.5 of
`PROJECT_PLAN.md`.

The step-order diagram claims *"the backend is fully functional (curl-able)
after S7 … so if you run out of steam mid-phase, you stop at a working
boundary."* For curl, true. For the app on your phone, **S6 breaks it and
nothing fixes it until S10**: `GET /notifications` starts requiring a bearer
token (401), `POST /notifications` starts requiring `recipient_email` and an
admin role, and the installed Flutter build sends neither.

That is a four-step window where `PROJECT_PLAN.md` §4.5's "every phase ships
something working end-to-end" is false at step granularity. It is an
acceptable trade — you cannot make notifications per-user and keep an
anonymous client working — but it should be stated in the step order rather
than discovered when you pick the phone up after S6. Add one line: *"S6
breaks the installed app; the device is expected to be unusable from S6 until
S10. Backend-only verification (curl) is the only check between those two
points."*

---

### F16 [SHOULD-FIX] — both new foreign keys are unindexed

**Amends:** §5's `0003` and `0004`, S5, S6.

Postgres indexes the *referenced* side of a foreign key (it has to — it is the
primary key) but **not the referencing side**. So as written:

- `sessions.user_id` has no index. Nothing queries by it today, but
  `ON DELETE CASCADE` from `users` does a sequential scan of `sessions` per
  deleted user, and "log out all devices" (§9, a named future feature) is a
  query on exactly this column.
- `notifications.user_id` has no index — and `get_all_for_user(txn, user_id)`
  is a `WHERE user_id = $1`, which is the single most-executed query the
  phase adds. Plus the same CASCADE scan.

`sessions.token_hash` is fine: its `UNIQUE` constraint gives you the index
every authenticated request needs, for free. `users.email` likewise.

At current row counts none of this is measurable, which is why it is
SHOULD-FIX rather than BLOCKER. But it is two lines in files you are writing
anyway, it is the kind of thing that is invisible until it is a production
incident, and "a foreign key is not an index" is worth learning once:

```sql
CREATE INDEX IF NOT EXISTS sessions_user_id_idx      ON sessions (user_id);
CREATE INDEX IF NOT EXISTS notifications_user_id_idx ON notifications (user_id);
```

---

### F17 [NICE] — `domain/` says "no I/O", and `generate_session_token()` does I/O

**Amends:** §7's first structural bullet.

§7 defends putting `password.cpp` in `domain/` on the grounds that the rule is
*"no Crow, no pqxx, no I/O"* — not "no dependencies" — and that hashing is a
pure function of its input. Correct, and I agree with the placement.

But `domain/token.cpp`'s `generate_session_token()` calls `randombytes_buf`,
which reads the OS CSPRNG (`getrandom(2)` / `/dev/urandom`). That is I/O, and
the function is not pure: it returns something different every call and has no
injectable seam, so a unit test can assert nothing beyond "two calls differ"
and "the shape is right" — which is exactly what S2's test shape says, for
exactly this reason.

This is not a reason to move the file. It is a reason to **fix the stated
rule**, because a documented invariant that the code quietly violates is worse
than a looser rule honestly stated. Reword §7 (and `project_structure.md`) to:
*"`domain/` = no Crow, no pqxx, no network and no filesystem state — leaf
dependencies on the standard library, nlohmann/json and libsodium are fine,
including libsodium's CSPRNG."* Then log it in §10 so it is a decision rather
than a drift.

---

### F18 [NICE] — three small things to know before you write the handlers

**Crow turns an escaped exception into a 500 and logs `what()`.** Its default
exception handler catches everything from a handler, replies 500, and logs
`"An uncaught exception occurred: " << e.what()` to stderr
(`crow/routing.h:1850-1872`, called from `:1771-1778`) **[verified]**. Two
consequences: (a) an uncaught `pqxx::unique_violation` will **not** kill the
process, so F3 is about returning the right status, not about crash safety;
(b) §8's "never log secrets" rule is already half-satisfied for free, because
`pqxx::sql_error` carries the *statement text* and not the parameter values
(`pqxx/except.hxx:381-388`) **[verified]** — parameterized queries keep
passwords and tokens out of error output. Worth a sentence in §8: the
`pqxx::params` discipline buys secret-hygiene in logs, not just injection
safety.

**Call `app.validate()` before `app.handle_full()`** in the functional
fixture. `run()` does it for you (`crow/app.h:601`); an in-process test does
not get that for free.

**Add every new target to the sanitizer block.** `backend/CMakeLists.txt:88`
iterates a hardcoded list (`atenciosamente_core atenciosamente_server`), and
`tests/CMakeLists.txt:16-17` already warns that a flag mismatch makes
ASan abort at startup. The new `atenciosamente_http` library and
`tests_functional` executable both need adding, or the functional tier will
fail to start in the `dev` preset for reasons that look nothing like the
actual cause.

---

### F19 [NICE] — logout against an already-dead token returns 401, and the app must not treat that as an error

**Amends:** §6's `DELETE /sessions` row, S10.

`DELETE /sessions` authenticates first, so logging out with an expired or
already-deleted token returns `401`, not `204`. That is correct REST and
correct security. It is also the single most likely 401 a real user will hit:
open the app after 30 days, tap logout.

S10 wires logout as *"calls `DELETE /sessions`, clears storage, and returns to
`LoginScreen`"* and separately says *"handles a 401 from any call by bouncing
to login"*. Make sure those two rules compose in the right order: the logout
path must treat `401` as **success** (the server already has no session; clear
storage and go to login) rather than surfacing a SnackBar error for a logout
that in fact achieved exactly what the user asked for. One line in
`auth_client.dart`'s `logout`, worth being deliberate about.

---

### F20 [NICE] — the unit tier stops being fast, and the `dev` preset may make it much worse

**Amends:** S1's "keep the hash-call count low", `PROJECT_PLAN.md` §8.

`tests_unit` will now link libsodium (transitively — see §4's note on
PRIVATE linkage) and execute Argon2id. §8 characterizes the unit tier as
"fast, many"; this is the first unit test that costs tens of milliseconds per
assertion rather than microseconds. S1 already says to keep the call count
low, which is right.

One thing to **measure rather than assume [unverified]**: vcpkg builds
separate debug and release variants, and the `dev` preset
(`CMAKE_BUILD_TYPE=Debug`) links the debug build of libsodium. libsodium's
own `configure` normally injects `-O3`, but vcpkg sets `CFLAGS` itself, so the
debug variant may well be unoptimized — in which case Argon2id could be
several times slower under `--preset=dev` than under `--preset=ci`, and a
login in the dev container could take noticeably longer than the "tens of
milliseconds" §4 D1 quotes. Time one `hash_password()` call under both presets
in S1 and write the two numbers into the §10 entry. If `dev` turns out to be
painfully slow, the answer is fewer hash calls in the test — **not** lowering
the cost parameters per build configuration, which would make the dev
environment test something production does not do.

---

## 3. Fact-check summary

**Verified correct** (checked against the installed dependencies / the running
`db`, 2026-10-09):

| Claim | Where | Result |
|---|---|---|
| vcpkg port is `libsodium`; CMake package `unofficial-sodium`; target `unofficial-sodium::sodium` | S1's "⚠️ Verify, don't assume" | **Right.** `ports/libsodium/vcpkg.json` (v1.0.22#1) and `ports/libsodium/sodiumConfig.cmake.in` define exactly `unofficial-sodium::sodium`, installed as `share/unofficial-sodium/unofficial-sodiumConfig.cmake`. You can retire the uncertainty — but keep the instruction to re-check, since F8 means the resolved port floats. |
| `pqxx::params` + `txn.exec(sql, params)` is the non-deprecated shape | §10 2026-07-27, S3/S5 | **Right.** `exec(std::string_view, params const&, sl)` at `transaction_base.hxx:360`; `exec_params` is `[[deprecated]]` at `:744`. With `-Werror` the deprecated form is a build failure. |
| `pqxx::unique_violation` is the type to catch | S4 | **Right.** `except.hxx:766`, under `integrity_constraint_violation`. Include `<pqxx/except>`. |
| `now() + ($n * INTERVAL '1 day')` is valid parameterized SQL | D9 | **Right, no cast needed.** The parameter resolves to `double precision`; see F12 for the live `PREPARE` output and why `make_interval` is nicer. |
| libsodium links `PRIVATE` on a *static* library and still reaches the executables | §7, S1 | **Right, and worth understanding why.** A static library has no link step, so CMake records `PRIVATE` dependencies of a static library as `$<LINK_ONLY:...>` usage requirements: they propagate for *linking* (so `atenciosamente_server`, `tests_unit` and `tests_integration` all get `-lsodium`) but not for include directories. That is precisely the asymmetry you want versus libpqxx's PUBLIC — and it only works because `password.hpp` includes nothing from `<sodium.h>`, so keep that true. |
| Crow catches handler exceptions rather than terminating | implied throughout | **Right**, see F18. |
| `sodium_init()` behind `std::once_flag` is the right init discipline | §7, S1 | **Right in kind, broken in instance** — see F2. One flag per process, not one per translation unit. |

**Not verifiable here, verify during implementation [unverified]:**

- The libsodium signatures and constants. From memory they are
  `int crypto_pwhash_str(char out[crypto_pwhash_STRBYTES], const char* passwd,
  unsigned long long passwdlen, unsigned long long opslimit, size_t memlimit)`
  and
  `int crypto_pwhash_str_verify(const char* str, const char* passwd,
  unsigned long long passwdlen)`, both returning `0` on success and `-1` on
  failure, with `crypto_pwhash_STRBYTES == 128`,
  `crypto_pwhash_OPSLIMIT_INTERACTIVE == 2` and
  `crypto_pwhash_MEMLIMIT_INTERACTIVE == 67108864` (= 64 MiB, which matches
  §4 D1's "~64 MiB"). **libsodium is not installed in this container yet**, so
  I could not confirm a single one of those against a header. Read
  `vcpkg_installed/x64-linux/include/sodium/crypto_pwhash.h` in S1 and
  correct this table. Two things that follow regardless: `hash_password` must
  check the return value and throw on `-1` (it fails on memory exhaustion, not
  just on misuse), and the output buffer must be exactly
  `char[crypto_pwhash_STRBYTES]` — not a guessed size.
- Whether `crypto_pwhash_str_verify` is safe against a stored hash that is
  not a valid PHC string. S1's test shape ("garbage stored hash fails
  verification instead of crashing") is the right test; the expectation is a
  `-1` return, but confirm it rather than trusting it, since
  `verify_password` will be called with whatever is in the column.
- Argon2id wall time in this container, per preset. See F20.
- Whether libsodium's Argon2 allocates through `sodium_malloc` and therefore
  attempts `mlock`. It does not matter here (`ulimit -l` is `unlimited`
  **[verified]**) but it would matter under a container with a memlock limit,
  so note the number you see if you ever hit an unexplained hash failure.

**Factually wrong in the existing docs:**

1. **D4 reason 3** — "`authenticate(txn, req)` is callable from an integration
   test" is false in this build. F1.
2. **§7's `once_flag` claim** — "there's no ordering to get wrong" stops being
   true the moment S2 adds a second libsodium caller. F2.
3. **§8's "no maximum length"** — the premise (Argon2id has no input cap) does
   not justify the conclusion. F11.
4. **`PROJECT_PLAN.md` §10, 2026-04-23** — the asserted `builtin-baseline`
   pin does not exist in `vcpkg.json`. F8.
5. **S3's `created_at` instruction** — "reuse `notification_repository.cpp`'s
   `to_char(...)` expression" is not reusable as written:
   `kCreatedAtSelectExpr` and `parse_created_at` are both in an anonymous
   namespace in that `.cpp` (`:12-45`). Also, "the project never parses
   timestamps in C++" is not accurate — `parse_created_at` does exactly that.
   F7, decision 3.
6. **S6's file list** omits `tests/unit/notification_json_test.cpp`. F14.

---

## 4. Revised architecture

### Layers and files

```
backend/src/
├── domain/                            pure: no Crow, no pqxx, no network, no fs
│   ├── sodium.{hpp,cpp}               ensure_sodium_initialized()           new
│   ├── password.{hpp,cpp}             hash_password / verify_password /
│   │                                  dummy_password_hash                   new
│   ├── token.{hpp,cpp}                generate_session_token / hash_token   new
│   ├── bearer_token.{hpp,cpp}         extract_bearer_token                  new
│   ├── roles.hpp                      kRoleAdmin / kRoleRecipient           new
│   ├── user.hpp                       User / UserSummary                    new
│   ├── user_json.{hpp,cpp}            to_json(const UserSummary&)           new
│   ├── authenticated_user.hpp         AuthenticatedUser                     new
│   ├── auth_response.hpp              AuthResponse{token, UserSummary}      new
│   ├── auth_response_json.{hpp,cpp}   to_json(const AuthResponse&)          new
│   ├── access.{hpp,cpp}               notification_read_scope /
│   │                                  may_create_notifications              new
│   ├── register_user_request.{hpp,cpp}                                      new
│   ├── login_request.{hpp,cpp}                                             new
│   ├── notification.hpp               + user_id, + recipient_email      changed
│   ├── notification_json.{hpp,cpp}    serialize the new fields         changed
│   └── create_notification_request.{hpp,cpp}  + recipient_email        changed
│       (NOTE: no domain/session.hpp — see F7)
├── auth/                              NEW LAYER: policy that needs the DB,
│   └── authenticator.{hpp,cpp}        but not HTTP. authenticate_bearer()
├── db/                                unchanged
├── repository/                        SQL against an already-open pqxx::work&
│   ├── user_repository.{hpp,cpp}                                           new
│   ├── session_repository.{hpp,cpp}                                        new
│   └── notification_repository.{hpp,cpp}  + get_all_for_user, user_id  changed
├── handlers/                          Crow adapters. No decisions.
│   ├── http_errors.{hpp,cpp}          bad_request / unauthorized /
│   │                                  forbidden / conflict                  new
│   ├── auth.{hpp,cpp}                 register / login / logout +
│   │                                  the authenticate() adapter            new
│   └── notifications.{hpp,cpp}        guard + scope dispatch           changed
├── app.cpp                            + 3 routes; GET gains a request   changed
└── main.cpp                           + pool-size/concurrency warning   changed
```

### CMake targets

| Target | Kind | Sources | Links |
|---|---|---|---|
| `atenciosamente_core` | STATIC | `domain/` + `db/` + `repository/` + `auth/` | PUBLIC `nlohmann_json`, `libpqxx::pqxx`, `Threads`; **PRIVATE** `unofficial-sodium::sodium` |
| `atenciosamente_http` | STATIC **(new)** | `app.cpp`, `handlers/*.cpp` | PUBLIC `atenciosamente_core`, `Crow::Crow` |
| `atenciosamente_server` | EXE | `main.cpp` only | PRIVATE `atenciosamente_http` |
| `tests_unit` | EXE | `tests/unit/*.cpp` | `atenciosamente_core`, Catch2 |
| `tests_integration` | EXE | `tests/integration/**` | `atenciosamente_core`, `libpqxx::pqxx`, Catch2 |
| `tests_functional` | EXE **(new)** | `tests/functional/**` | `atenciosamente_http`, Catch2 |

All six go in the `ENABLE_SANITIZERS` block (F18).

### The auth path, exact signatures

```cpp
// src/domain/bearer_token.hpp
std::optional<std::string> extract_bearer_token(std::string_view authorization_header);

// src/domain/token.hpp
std::string generate_session_token();                 // 32 CSPRNG bytes -> 64 hex chars
std::string hash_token(const std::string& token);     // SHA-256 -> 64 hex chars

// src/repository/session_repository.hpp
namespace session_repository {
void insert(pqxx::work& txn, std::int64_t user_id,
            const std::string& token_hash, int ttl_days);
std::optional<AuthenticatedUser> find_authenticated_user(pqxx::work& txn,
                                                         const std::string& token_hash);
bool delete_by_token_hash(pqxx::work& txn, const std::string& token_hash);
}

// src/auth/authenticator.hpp   (in atenciosamente_core — integration-testable)
std::optional<AuthenticatedUser> authenticate_bearer(pqxx::work& txn,
                                                     std::string_view authorization_header);
//   = extract_bearer_token(header)
//     |> hash_token
//     |> session_repository::find_authenticated_user
//   nullopt for: no header, wrong scheme, empty token, unknown token, expired row.

// src/handlers/auth.hpp        (in atenciosamente_http — the Crow seam)
inline std::optional<AuthenticatedUser> authenticate(pqxx::work& txn,
                                                     const crow::request& req) {
    return authenticate_bearer(txn, req.get_header_value("Authorization"));
}

inline constexpr int kSessionTtlDays = 30;

crow::response handle_register_user(ConnectionPool& pool, const crow::request& req);
crow::response handle_login(ConnectionPool& pool, const crow::request& req);
crow::response handle_logout(ConnectionPool& pool, const crow::request& req);

// src/domain/access.hpp        (pure — unit-testable)
enum class NotificationScope { OwnOnly, All };
NotificationScope notification_read_scope(const AuthenticatedUser& user);
bool may_create_notifications(const AuthenticatedUser& user);
```

### Endpoint flows, with the lease boundary marked

`|---|` marks the span during which a pooled connection is held.

```
POST /users  (register)
  parse JSON body                                     no lease
  parse_register_user_request  (lowercase, caps)       no lease   -> 400
  hash_password(...)                     ~60 ms CPU    no lease
  |-- acquire -> work ---------------------------------------------|
  |     try  user_repository::insert(txn, email, hash)             |
  |     catch pqxx::unique_violation -> return 409   (txn poisoned)|
  |     session_repository::insert(txn, id, hash_token(tok), 30)   |
  |     txn.commit()                                               |
  |---------------------------------------------------- release ---|
  201 {token, user}

POST /sessions  (login)
  parse JSON body / parse_login_request                no lease   -> 400
  |-- acquire -> work --|
  |  find_by_email      |                 ~1 ms
  |----------- release -|   (read-only; no commit, work rolls back)
  verify_password(plain, user ? user->password_hash
                              : dummy_password_hash())            <- F9
                                         ~60 ms CPU    no lease
  !user || !ok -> 401 {"error":"invalid email or password"}        no lease
  generate_session_token()                             no lease
  |-- acquire -> work --|
  |  session_repository::insert(...); txn.commit()                 |
  |  catch pqxx::foreign_key_violation -> 401                      |
  |----------- release -|
  201 {token, user}       (byte-identical shape to register)

DELETE /sessions  (logout)
  |-- acquire -> work ---------------------------------------------|
  |  authenticate(txn, req)            -> nullopt -> 401           |
  |  session_repository::delete_by_token_hash(...); txn.commit()    |
  |---------------------------------------------------- release ---|
  204

GET /notifications
  |-- acquire -> work ---------------------------------------------|
  |  authenticate(txn, req)            -> nullopt -> 401           |
  |  switch (notification_read_scope(*user)) {                     |
  |    All     -> notification_repository::get_all(txn)            |
  |    OwnOnly -> notification_repository::get_all_for_user(txn,   |
  |                                               user->id) }      |
  |---------------------------------------------------- release ---|
  200 [ ... ]            (read-only: no commit, same as today)

POST /notifications
  parse JSON + parse_create_notification_request       no lease   -> 400
  |-- acquire -> work ---------------------------------------------|
  |  authenticate(txn, req)            -> nullopt -> 401           |
  |  !may_create_notifications(*user)  -> 403                      |
  |  user_repository::find_by_email(txn, recipient_email)          |
  |                                     -> nullopt -> 400          |
  |  notification_repository::insert(txn, recipient_id, ...)       |
  |  txn.commit()                                                  |
  |---------------------------------------------------- release ---|
  201 {notification}
```

Two rules to read off that diagram, which is the whole point of drawing it:
**anything CPU-bound happens outside the braces**, and **a lease is never held
across a call that can take longer than a query**.

### Schema (all three migrations, revised)

```sql
-- 0002_create_users.sql
CREATE TABLE IF NOT EXISTS users (
    id             BIGINT      GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    email          TEXT        NOT NULL UNIQUE CHECK (email = lower(email)),
    password_hash  TEXT        NOT NULL,
    role           TEXT        NOT NULL CHECK (role IN ('admin', 'recipient')),
    created_at     TIMESTAMPTZ NOT NULL DEFAULT now()
);
```

```sql
-- 0003_create_sessions.sql
CREATE TABLE IF NOT EXISTS sessions (
    id          BIGINT      GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    user_id     BIGINT      NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    token_hash  TEXT        NOT NULL UNIQUE,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
    expires_at  TIMESTAMPTZ NOT NULL
);

CREATE INDEX IF NOT EXISTS sessions_user_id_idx ON sessions (user_id);
```

`0004` is in F6. The `UNIQUE` on `token_hash` is the index every
authenticated request uses; the extra index is for the CASCADE and for §9's
future "log out all devices".

---

## 5. Revised step-by-step

Twelve steps instead of eleven, re-cut so that (a) nothing is written twice,
(b) the auth guard is testable the moment it exists, and (c) the functional
tier arrives before the code that needs it. The convention from the
implementation doc still applies: one step per conversation, each ending in one
commit, `PROJECT_PLAN.md` + both Phase 4 docs + this one attached every time.

Where a step is unchanged from the original, it says so and only the delta is
listed. Paste-in prompts are given only for the steps that are genuinely new
or substantially re-cut.

| # | Goal | Green means |
|---|---|---|
| S1 | Pin vcpkg, add libsodium, sodium init + password hashing | `ctest -R '^unit/'` |
| S2 | Token primitives + bearer-header parsing | `ctest -R '^unit/'` |
| S3 | `users` table, user types, user repository | `^unit/` + `^integration/` |
| S4 | `sessions` table + session repository | `^unit/` + `^integration/` |
| S5 | `auth/authenticator` — the guard, with tests | `^unit/` + `^integration/` |
| S6 | HTTP library split + functional tier + CI job (no new features) | all three tiers |
| S7 | `POST /users`, `POST /sessions`, `DELETE /sessions` | all three tiers |
| S8 | Notifications become per-user and role-gated | all three tiers |
| S9 | Config, CI, first-admin bootstrap | CI green on a cold cache |
| S10 | Mobile: models, `api_config`, `auth_client` | `flutter analyze` |
| S11 | Mobile: login + register screens | `flutter analyze` + a real signup from the phone |
| S12 | Mobile: `AuthGate`, session wiring, logout | full manual device script |
| S13 | Close the phase: decision log, checklist, docs | — |

(That is thirteen rows; S10–S12 are the original S8–S10 unchanged, and S13 is
the original S11 unchanged. If you want to spend fewer conversations, the two
safe merges are S4+S5 and S10+S11 — do **not** merge S6 into S7, which is the
whole point of the re-cut.)

---

### S1 — vcpkg baseline, libsodium, sodium init, password hashing

Replaces the original S1.

- `backend/vcpkg.json` — **add `builtin-baseline` first, as a separate
  change** (F8), then add `"libsodium"`.
- `backend/CMakeLists.txt` — `find_package(unofficial-sodium CONFIG REQUIRED)`;
  add `src/domain/sodium.cpp` and `src/domain/password.cpp` to
  `atenciosamente_core`; link `unofficial-sodium::sodium` **PRIVATE**.
- `src/domain/sodium.{hpp,cpp}` (new) — `ensure_sodium_initialized()`, one
  process-wide `std::once_flag` (F2).
- `src/domain/password.{hpp,cpp}` (new) — `hash_password`, `verify_password`,
  `dummy_password_hash` (F9). Header includes only `<string>`.
- `tests/unit/password_test.cpp` (new) + `tests/CMakeLists.txt`.
- **Correct `PROJECT_PLAN.md` §10's 2026-04-23 baseline entry** in the same
  commit as the pin.
- **Record:** D1; `INTERACTIVE` over `MODERATE`/`SENSITIVE`; PRIVATE-on-a-
  static-library linkage and why it still reaches the executables; one
  process-wide init TU; `dummy_password_hash` derived rather than hardcoded;
  the two measured hash timings (`dev` vs `ci`, F20).
- **Green:** `ctest --preset=dev -R '^unit/'`, no database.

**Prompt**
```
Use the backend subagent to implement Step S1 of Phase 4 (revised).
Attached: PROJECT_PLAN.md, both Phase 4 docs, and
PHASE_4_REVIEW_AND_ADJUSTMENTS.md — the review's findings F2, F8, F9 and F20
change this step. Recent history:
[GIT LOG HERE]

Goal: password hashing as pure domain code, on a pinned dependency set.
1. FIRST, and as its own change: backend/vcpkg.json has no builtin-baseline
   (review F8) even though PROJECT_PLAN.md §10's 2026-04-23 entry claims it
   does. Add the baseline, correct that §10 entry, and tell me the commit
   you pinned to. Only then add libsodium.
2. find_package(unofficial-sodium CONFIG REQUIRED) + link
   unofficial-sodium::sodium PRIVATE on atenciosamente_core. The review
   verified those names against the installed port, but re-check them anyway
   now that the baseline is pinned. Explain why PRIVATE works on a STATIC
   library and still gets libsodium's symbols into the server and test
   binaries, and why libpqxx had to be PUBLIC instead.
3. src/domain/sodium.{hpp,cpp}: ensure_sodium_initialized(), ONE
   process-wide std::once_flag, throwing if sodium_init() returns < 0.
   Review F2: S2 will also need libsodium, and a second once_flag in a
   second .cpp would allow two concurrent sodium_init() calls — which is the
   exact hazard the once_flag was supposed to prevent.
4. src/domain/password.{hpp,cpp}: hash_password(), verify_password(), and
   dummy_password_hash() returning a lazily-computed function-local static
   produced by hash_password() itself (review F9 — not a hardcoded PHC
   literal, so it can't drift from the current cost parameters). Check
   crypto_pwhash_str's return value and throw on failure. password.hpp
   includes only <string>.
5. Read sodium/crypto_pwhash.h and tell me the REAL signatures and the real
   values of crypto_pwhash_STRBYTES / OPSLIMIT_INTERACTIVE /
   MEMLIMIT_INTERACTIVE — the review could not verify these, it only
   recalled them.
6. tests/unit/password_test.cpp: same password twice gives different strings
   that both verify; wrong password fails; a garbage stored hash fails
   without crashing; the hash starts with "$argon2id$". Keep it to ~3 hash
   calls.
7. Time one hash_password() call under --preset=dev and under --preset=ci and
   report both numbers (review F20: the Debug libsodium may be unoptimized).
Explain what's actually inside the Argon2id output string and why that means
no salt column.
When done: record the decisions listed in the review's revised S1 in
PROJECT_PLAN.md §10 and commit in `Scope (Tag): summary` style.
```

---

### S2 — Token primitives and bearer-header parsing

Extends the original S2 with `extract_bearer_token` (F1), which must exist
before S5.

- `src/domain/token.{hpp,cpp}` (new) — `generate_session_token()`,
  `hash_token()`; both call `ensure_sodium_initialized()` first.
- `src/domain/bearer_token.{hpp,cpp}` (new) — `extract_bearer_token`.
- `tests/unit/token_test.cpp`, `tests/unit/bearer_token_test.cpp` (new).
- **Reword `domain/`'s purity rule** in the architecture doc §7 and in
  `reference/project_structure.md` (F17), and log it.
- **Record:** D3; the CSPRNG-is-I/O wording fix.
- **Green:** `^unit/`. `bearer_token_test.cpp` is the cheap half of the
  guard's coverage: empty header, `"Bearer"` alone, `"Bearer "` with nothing
  after it, `"bearer abc"` (must work — the scheme is case-insensitive),
  `"Basic abc"` (must not), surrounding whitespace, and a token with an
  embedded space.

---

### S3 — `users` table, user types, user repository

The original S3, with F7 and F10 applied.

- `migrations/0002_create_users.sql` — as in §4 above, **including
  `CHECK (email = lower(email))`**.
- `src/domain/user.hpp`, `src/domain/authenticated_user.hpp`,
  `src/domain/roles.hpp`, `src/domain/user_json.{hpp,cpp}` — exact definitions
  in F7. **No `created_at` on any of them**, and therefore no `to_char` and no
  timestamp parsing in this repository.
- `src/repository/user_repository.{hpp,cpp}` — `insert` (role forced to
  `kRoleRecipient`), `find_by_email -> optional<User>`,
  `find_by_id -> optional<UserSummary>`.
- `tests/integration/repository/user_repository_test.cpp` — round trip;
  **`REQUIRE_THROWS_AS(..., pqxx::unique_violation)`** on the duplicate (F3);
  unknown email is `nullopt`; role is `recipient`; and a mixed-case email
  rejected by the new CHECK.
- **Record:** D5; the `User`/`UserSummary` split; lowercase-in-C++ **plus** the
  database-side CHECK that backs it; no timestamps in the user types and why.

---

### S4 — `sessions` table and session repository

**New position** (was folded into the old S5) — this is the fix for F13.
Still no HTTP.

- `migrations/0003_create_sessions.sql` — §4 above, with
  `sessions_user_id_idx` (F16).
- `src/repository/session_repository.{hpp,cpp}` — the three functions in §4.
  `insert` writes `expires_at` as `now() + make_interval(days => $3)` (F12).
- `src/domain/auth_response.hpp`, `src/domain/auth_response_json.{hpp,cpp}` —
  the shared `{token, user}` body, created now so S7 never has to change a
  response shape.
- `tests/integration/repository/session_repository_test.cpp` — a row written
  with a **past** `expires_at` is not found; a fresh one is;
  `delete_by_token_hash` makes it unfindable and returns `true`, then `false`;
  an unknown hash is `nullopt`.
- `tests/unit/auth_response_json_test.cpp` — the body has exactly
  `token` and `user{id,email,role}`, and **no `password_hash`**.
- **Record:** D2; D9 (30-day absolute expiry, `make_interval`, constant in C++
  and arithmetic in SQL); `domain/session.hpp` deleted from the plan and why
  (F7).

**Prompt**
```
Use the backend subagent to implement Step S4 of Phase 4 (revised).
Attached: PROJECT_PLAN.md, both Phase 4 docs, PHASE_4_REVIEW_AND_ADJUSTMENTS.md.
This step was pulled EARLIER than the original plan (review F13) so that the
first endpoint can ship its final {token, user} response shape instead of
being rewritten one step later. Recent history:
[GIT LOG HERE]

Goal: the sessions table and its repository. Still no HTTP in this step.
- migrations/0003_create_sessions.sql per the review's §4: token_hash UNIQUE,
  user_id FK ON DELETE CASCADE, expires_at NOT NULL, AND an explicit
  sessions_user_id_idx — Postgres does not index the referencing side of a
  foreign key (review F16).
- src/repository/session_repository.{hpp,cpp}, free functions over
  pqxx::work&: insert(txn, user_id, token_hash, ttl_days) writing expires_at
  as now() + make_interval(days => $3) — the review verified against our own
  Postgres 16 that this types the parameter as integer, whereas
  $3 * INTERVAL '1 day' types it as double precision (F12);
  find_authenticated_user(txn, token_hash) -> optional<AuthenticatedUser>,
  JOINing users and filtering expires_at > now() (renamed from the
  architecture doc's find_valid_by_token_hash so the name matches what it
  returns, review F7); delete_by_token_hash(txn, token_hash) -> bool.
- src/domain/auth_response.hpp + auth_response_json.{hpp,cpp}: the shared
  {token, user} response body that BOTH register and login will return. Per
  review F7 there is no domain/session.hpp — nothing server-side holds a
  session as a value.
- tests/integration/repository/session_repository_test.cpp: write a row with
  a PAST expires_at directly and assert it is NOT found; a fresh one is;
  delete makes it unfindable; unknown hash -> nullopt. Reuse the existing
  never-commit rollback isolation verbatim.
- tests/unit/auth_response_json_test.cpp: the serialized body has exactly
  token + user{id,email,role} and no password_hash anywhere.
Explain why we can revoke a token instantly here when a JWT design could not.
When done: record D2 and D9 in PROJECT_PLAN.md §10 and commit in
`Scope (Tag): summary` style.
```

---

### S5 — `auth/authenticator` — the guard itself

**New step.** This is F1's restructuring, placed deliberately *before* any
Crow code exists so the untestable version cannot be written by accident.

- `src/auth/authenticator.{hpp,cpp}` (new) — `authenticate_bearer`, added to
  `atenciosamente_core`. Must not include Crow.
- `backend/CMakeLists.txt` — the new source; note `src/auth/` as a new layer
  in `reference/project_structure.md`.
- `tests/integration/auth/authenticator_test.cpp` (new) — the list in F1,
  including the raw-token-stored-verbatim-does-not-authenticate case.
- **Record:** the `src/auth/` layer and its rule ("needs the DB, not HTTP");
  D4 restated as *core function + one-line Crow adapter*, with the reason the
  original shape was untestable (`handlers/` lives on the executable; no test
  target links Crow).
- **Green:** `^unit/` + `^integration/`. At the end of this step the entire
  authentication decision has automated coverage and the server does not yet
  know it exists.

**Prompt**
```
Use the backend subagent to implement Step S5 of Phase 4 (revised).
Attached: PROJECT_PLAN.md, both Phase 4 docs, PHASE_4_REVIEW_AND_ADJUSTMENTS.md.
This step did not exist in the original plan: review finding F1 showed that
authenticate() as designed (in handlers/, on the atenciosamente_server
executable) cannot be reached by ANY test target, because neither tests_unit
nor tests_integration links Crow. Recent history:
[GIT LOG HERE]

Goal: the authentication guard, as Crow-free core code with real tests.
- New layer src/auth/ for "policy that needs the database but not HTTP" —
  it takes a pqxx::work& so it can't be domain/, and it contains no SQL and
  makes a policy decision so it shouldn't be repository/.
- src/auth/authenticator.{hpp,cpp}, compiled into atenciosamente_core:
    std::optional<AuthenticatedUser>
    authenticate_bearer(pqxx::work& txn, std::string_view authorization_header);
  = extract_bearer_token (S2) -> hash_token (S2) ->
    session_repository::find_authenticated_user (S4). It must NOT include
  any Crow header.
- tests/integration/auth/authenticator_test.cpp: a valid token resolves to
  the right user and role; an expired session row resolves to nullopt; a
  deleted row resolves to nullopt; an unknown-but-well-formed token resolves
  to nullopt; an empty header resolves to nullopt; and — the important one —
  a session row whose token_hash column holds the RAW token does not
  authenticate (that's the regression test for "did we remember to hash
  before looking up").
- Update Documentation/reference/project_structure.md's backend tree with the
  new src/auth/ layer.
Do NOT write any Crow code in this step. The one-line Crow adapter comes in
S7.
Explain what the raw-token test is actually protecting against.
When done: record the src/auth/ layer and the revised D4 shape in
PROJECT_PLAN.md §10 and commit in `Scope (Tag): summary` style.
```

---

### S6 — HTTP library split and the functional test tier

**New step.** No new features at all. This is F4's infrastructure, built and
proven against endpoints whose behaviour you already know is correct, so that
when S7 and S8 add security behaviour the tier is already trustworthy.

- `backend/CMakeLists.txt` — new `atenciosamente_http` STATIC library
  (`app.cpp` + `handlers/notifications.cpp`, linking `atenciosamente_core` and
  `Crow::Crow` PUBLIC); `atenciosamente_server` becomes `main.cpp` only; both
  new targets added to the `ENABLE_SANITIZERS` list (F18).
- `backend/tests/CMakeLists.txt` — `tests_functional` target,
  `catch_discover_tests(... TEST_PREFIX "functional/")`.
- `backend/tests/functional/support/test_app.{hpp,cpp}` (new) — the fixture:
  owns a `ConnectionPool`, a `crow::SimpleApp`, calls `setup_routes` then
  `app.validate()`, and offers `get`/`post`/`del` helpers that build a
  `crow::request` and call `app.handle_full`. Plus `unique_test_email()` and
  the `DELETE FROM users WHERE email LIKE 'func-%@test.local'` teardown.
- `backend/tests/functional/notifications_test.cpp` (new) — the *existing*
  contract: `GET /notifications` is 200 and a JSON array; `POST` with a valid
  body is 201 and echoes title/body; `POST` with a non-JSON body is 400;
  `POST` with a missing field is 400; `GET /` is 200 "hello".
- `.github/workflows/backend-ci.yml` — a third job, `functional`, cloned from
  `integration` with `-R '^functional/'`.
- `.github/workflows/README.md` — **update the backend diagram in the same
  commit** to show three tier jobs.
- **Record:** the functional tier finally landing, three phases after §8
  promised it, and *why now* (Phase 4 is the first phase whose interesting
  logic lives in the handler layer); in-process `handle_full` dispatch instead
  of a socket, and what that does and does not cover (it exercises Crow's
  router, the handlers and real SQL; it does not exercise the HTTP parser, the
  socket layer or `main()`); prefix-scoped cleanup instead of rollback
  isolation, with the explicit reason that handlers commit; and the
  `atenciosamente_http` extraction finally making true the §10 2026-04-25
  claim that `app.cpp` can link into a test target.
- **Green:** all three of `^unit/`, `^integration/`, `^functional/`, locally
  and in CI, with **no behaviour change anywhere** — `git diff` touches only
  CMake, CI, docs and new test files.

**Prompt**
```
Use the backend subagent to implement Step S6 of Phase 4 (revised).
Attached: PROJECT_PLAN.md, both Phase 4 docs, PHASE_4_REVIEW_AND_ADJUSTMENTS.md.
This step is new: review finding F4 argues Phase 4 is where PROJECT_PLAN.md
§8's long-promised functional tier has to arrive, because every interesting
behaviour this phase adds (401/403/409, the role branch) lives in handlers/,
which no test tier can currently reach. Recent history:
[GIT LOG HERE]

Goal: build the functional tier. NO new features and no behaviour changes —
this step must leave every existing response byte-identical.
- CMakeLists.txt: extract app.cpp + handlers/notifications.cpp into a new
  STATIC library atenciosamente_http that links atenciosamente_core and
  Crow::Crow PUBLIC. atenciosamente_server becomes src/main.cpp only. This
  finally makes true the PROJECT_PLAN.md §10 claim from 2026-04-25 that the
  main.cpp/app.cpp split lets app.cpp link into a test target. Add BOTH new
  targets to the ENABLE_SANITIZERS foreach — a sanitizer flag mismatch makes
  ASan abort at startup (review F18).
- tests/CMakeLists.txt: a tests_functional executable linking
  atenciosamente_http + Catch2, with TEST_PREFIX "functional/".
- tests/functional/support/test_app.{hpp,cpp}: a fixture owning a
  ConnectionPool and a crow::SimpleApp, calling setup_routes() then
  app.validate() (run() normally does validate() for us — an in-process test
  doesn't get that for free), with get/post/del helpers that build a
  crow::request by hand and call app.handle_full(req, res). No socket, no
  port, no server thread.
- Isolation: handlers call txn.commit(), so the per-test rollback trick the
  integration tier uses CANNOT work here — that's the real reason this is a
  third tier. Use a random per-test email of the form
  func-<hex>@test.local plus a teardown that runs
  DELETE FROM users WHERE email LIKE 'func-%@test.local'. Do NOT TRUNCATE —
  it must not delete the admin account I create by hand later.
- tests/functional/notifications_test.cpp: assert today's known-good
  contract only — GET /notifications is 200 + a JSON array, POST with a valid
  body is 201 and echoes title/body, POST with non-JSON is 400, POST with a
  missing field is 400, GET / is 200 "hello".
- .github/workflows/backend-ci.yml: a third job `functional`, cloned from
  `integration`, filtering -R '^functional/'. And update the backend diagram
  in .github/workflows/README.md in the SAME commit.
Explain what this tier does and does not cover versus a real-socket E2E test,
and why committing tests need a different isolation strategy from rolling-back
ones.
When done: record the tier decision in PROJECT_PLAN.md §10 and commit in
`Scope (Tag): summary` style.
```

---

### S7 — `POST /users`, `POST /sessions`, `DELETE /sessions`

Merges the endpoint halves of the original S4 and S5, now with no response
shape to rewrite.

- `src/domain/register_user_request.{hpp,cpp}`,
  `src/domain/login_request.{hpp,cpp}` (new) — the `ValidationResult` shape
  from `create_notification_request.hpp:13-16`; email lowercased with the
  `unsigned char` cast (F10); the 254-byte email and 1024-byte password caps
  on **both** validators, the 8-character minimum on registration only (F11);
  **no `role` field ever read** (D6).
- `src/handlers/http_errors.{hpp,cpp}` (new) — move `bad_request` out of
  `notifications.cpp`'s anonymous namespace (`notifications.cpp:11-24`); add
  `unauthorized` (with `WWW-Authenticate: Bearer`), `forbidden`, `conflict`.
- `src/handlers/auth.{hpp,cpp}` (new) — the three handlers with the lease
  boundaries exactly as diagrammed in §4; `catch (const pqxx::unique_violation&)`
  scoped to the user insert (F3); the `authenticate()` one-line adapter;
  `kSessionTtlDays`.
- `src/app.cpp` — the three routes.
- `tests/unit/register_user_request_test.cpp`,
  `tests/unit/login_request_test.cpp` (new).
- `tests/functional/auth_test.cpp` (new) — **this is the step that pays off
  S6.** 201 on register with the `{token, user}` body; 409 on a duplicate;
  400 on a short password; a body containing `"role":"admin"` still produces a
  `recipient`; 201 on login; a login with an unknown email and a login with a
  wrong password return **byte-identical** status and body (F9); 401 carries
  `WWW-Authenticate: Bearer`; logout is 204 and the same token then 401s.
- **Record:** D6; the `409`-not-generic asymmetry; catch-not-pre-check with
  the poisoned-transaction consequence; the dummy-hash timing defense and why
  it is derived rather than hardcoded; the lease-scoping rule from F5 as a
  general handler rule, not a login special case.
- **Green:** all three tiers. The revoke-a-token test is the concrete thing
  D2 claims JWT could not do — assert it, don't just curl it.

---

### S8 — Notifications become per-user and role-gated

The original S6, with F6, F14 and F4's policy functions applied.

- `migrations/0004_add_user_id_to_notifications.sql` — **the non-destructive
  five-statement form from F6**, comment and all, including
  `notifications_user_id_idx`.
- `src/domain/access.{hpp,cpp}` (new) — `notification_read_scope`,
  `may_create_notifications`; the only place `kRoleAdmin` is compared.
- `src/domain/notification.hpp`, `notification_json.{hpp,cpp}`,
  `create_notification_request.{hpp,cpp}` — the new fields.
- `src/repository/notification_repository.{hpp,cpp}` — `insert` takes
  `user_id`; add `get_all_for_user`; both reads `JOIN users` for
  `recipient_email` (D8).
- `src/handlers/notifications.{hpp,cpp}` — guard, then dispatch on
  `notification_read_scope` / `may_create_notifications`.
- `src/app.cpp` — the `GET /notifications` lambda now takes
  `const crow::request&`.
- Tests updated: `tests/unit/notification_json_test.cpp` **(F14 — not in the
  original list; it will not compile otherwise)**,
  `tests/unit/create_notification_request_test.cpp`, new
  `tests/unit/access_test.cpp`,
  `tests/integration/repository/notification_repository_test.cpp` (all four
  existing `TEST_CASE`s change — `insert` gains a parameter), and
  `tests/functional/notifications_test.cpp` gains the auth matrix: no token
  401; recipient `POST` 403; admin `POST` 201; recipient `GET` sees only its
  own rows; admin `GET` sees both; unknown `recipient_email` 400.
- **Add to the step order note:** S8 breaks the installed Flutter app and it
  stays broken until S12 (F15).
- **Record:** the backfill-then-`SET NOT NULL` migration shape over an
  unconditional `DELETE`; `401` vs `403`; D8; D7's deliberate
  "no such recipient" leak to an authenticated admin; the authorization
  decision extracted into `domain/access.hpp` so the role branch is
  unit-testable.

---

### S9–S13

- **S9** = the original **S7** (config, CI, first admin), plus: confirm all
  three CI jobs pass on a cold vcpkg cache, and that `0002`–`0004` apply in
  both the integration and functional jobs. Decide the session-TTL env-var
  question (§6, question 3).
- **S10, S11, S12** = the original **S8, S9, S10** unchanged, except:
  S10's `auth_client.dart` must treat a `401` from `logout` as success (F19),
  and the `{token, user}` shape it consumes is now stable from S4 onward.
- **S13** = the original **S11** unchanged, plus: fold this review's accepted
  findings into the architecture doc so the two files stop disagreeing, and
  tick §1's checklist honestly — the functional tier means §1's
  "Unit tests cover … integration tests cover …" line can be strengthened to
  name the endpoint behaviours that are now covered too.

---

## 6. Open questions for the human

These are yours to decide, not mine.

1. **Does `src/auth/` get to be a fifth layer?** F1 needs
   `authenticate_bearer` somewhere that is in `atenciosamente_core` but is
   neither `domain/` (it takes a `pqxx::work&`) nor `repository/` (it holds
   policy, not SQL). I recommend a new `src/auth/` folder, which also gives
   Phase 4's "whose notification is due?" logic a home. The alternative is
   putting the file in `handlers/` but compiling *that one file* into
   `atenciosamente_core` — smaller diff, but it breaks the invariant that
   every folder maps to a target and that `handlers/` means "Crow". Your call
   on whether a new layer is worth it; the testability requirement is not
   negotiable either way.

2. **`AuthenticatedUser` as its own struct, or reuse `UserSummary`?** The
   fields are identical today (F7, decision 2). I argued for the distinct
   type because it names a different concept and will diverge, but "two
   five-line structs with the same members" is a fair thing to dislike, and
   "dumb data" arguably favours one struct. If you reuse `UserSummary`, note
   that `to_json(UserSummary)` then accepts the authenticated caller, which
   weakens the F7 decision-2 argument slightly but breaks nothing.

3. **Is the 30-day TTL a C++ constant or an env var?** The original S7 asks
   this and I deliberately have not answered it. The `connection.cpp`
   `read_env` / `main.cpp` `read_pool_size` split says: required things throw,
   tuning things get a default. A session TTL is arguably tuning — but it is
   also a *security* parameter, and making it env-configurable means a typo in
   `.env` can silently create 1-day or 3000-day sessions with nothing failing
   loudly. My weak preference is to keep it a C++ constant for Phase 4 and
   revisit if you ever want different values per environment, but it is a
   genuine judgement call about where the "config from env" principle stops.

4. **Do you want the functional tier now, or the cheap version?** F4 gives
   both: the full tier (two extra steps, one CMake restructure, one CI job,
   one new isolation strategy) or just `domain/access.hpp` making the role
   branch unit-testable (fifteen lines, no infrastructure). I recommend the
   full tier *because this is the access-control phase* and because §8 has
   promised it for three phases — but it is a real cost on a learning project
   and "not yet, and here's the §10 entry saying why" is a legitimate answer.
   If you decline, please still take `domain/access.hpp`.

5. **Keep the `DELETE` in `0004` at all?** F6's version only deletes rows it
   cannot adopt, which on your dev database means "only if you have not
   registered yet". If you would rather the migration never deletes anything,
   the alternative is to make it fail loudly instead
   (`RAISE EXCEPTION` when un-adoptable rows remain) and force you to resolve
   it by hand. Safer, less convenient, and arguably the more honest default
   for a migration that will one day run against data you care about.

6. **Should `POST /users` stay open?** Not relitigating public
   self-registration — but §8 names the absence of rate limiting as the
   biggest live gap and pairs it with exactly this decision. Since rung 3 of
   the deployment ladder is already a prerequisite for leaving the LAN, is
   "rate limiting lands with the reverse proxy, in the same step" a commitment
   you want written into `PROJECT_PLAN.md` §7 now, so the two cannot drift
   apart?
