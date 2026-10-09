# Phase 4 — Users & Access Control: Implementation

> The **work breakdown** for Phase 4: eleven steps, each one focused
> conversation ending in one commit, each with a ready-to-paste prompt.
> This document does not argue for the design — that's
> [`PHASE_4_USERS_AND_ACCESS_ARCHITECTURE.md`](PHASE_4_USERS_AND_ACCESS_ARCHITECTURE.md),
> where every decision (referenced below as **D1**–**D11**) is justified
> against its runner-up.
>
> Attach [`PROJECT_PLAN.md`](../PROJECT_PLAN.md) and both Phase 4 docs to
> every step.

---

## How to run a step

1. Finish and **commit** the previous step first. One step per conversation.
2. Open a new conversation **in the WSL repo** (`~/projects/Atenciosamente`).
3. Attach `PROJECT_PLAN.md` + both Phase 4 docs.
4. Replace `[GIT LOG HERE]` with `git log --oneline -10`.
5. Paste the step's prompt.
6. Commit as `Scope (Tag): summary` — single line, no body, no trailers.

Backend work goes to the `backend` subagent, mobile work to the `frontend`
subagent. Build/test commands, for reference:

```bash
# backend, inside the dev container (docker compose exec backend bash)
cmake --preset=dev && cmake --build --preset=dev
ctest --preset=dev                    # all tiers
ctest --preset=dev -R '^unit/'        # fast tier only
./scripts/migrate.sh                  # apply pending migrations

# mobile, on the host
./run_dev.sh
```

---

## Step order, and why it's this order

```
  S1 password hashing ──┐
  S2 token primitives ──┤                     (pure domain, no DB, no HTTP)
                        ▼
  S3 users table + repository                  (DB, no HTTP)
                        ▼
  S4 POST /users (register) ──────┐            (first endpoint; proves S1+S3)
                                  ▼
  S5 sessions + login/logout + authenticate()  (proves S2; builds the guard)
                                  ▼
  S6 /notifications gains ownership + roles    (uses the guard; breaking change)
                                  ▼
  S7 env + CI + first-admin bootstrap          (backend is now usable end-to-end)
                                  ▼
  S8 mobile: models + api_config + auth_client ─┐
  S9 mobile: login + register screens          ─┤ (app becomes usable)
  S10 mobile: AuthGate + session wiring        ─┘
                                  ▼
  S11 docs: decision log + checklist
```

Each step builds and tests green on its own. The backend is fully functional
(curl-able) after **S7**, before any Flutter work starts — so if you run out
of steam mid-phase, you stop at a working boundary.

---

## S1 — Password hashing (`domain/password`)

Add libsodium and wrap Argon2id in two free functions. No DB, no HTTP.

- **Files:**
  - `backend/vcpkg.json` — add the libsodium dependency.
  - `backend/CMakeLists.txt` — `find_package(...)` + add
    `src/domain/password.cpp` to `atenciosamente_core` + link libsodium
    **PRIVATE** (D1/§7: `password.hpp` includes only `<string>`, so the
    dependency doesn't leak into a public header — the mirror image of why
    libpqxx is PUBLIC).
  - `backend/src/domain/password.{hpp,cpp}` (new) —
    `std::string hash_password(const std::string& plain);`
    `bool verify_password(const std::string& plain, const std::string& hash);`
    Internally: `crypto_pwhash_str` / `crypto_pwhash_str_verify` with
    libsodium's `INTERACTIVE` ops/mem limits, and a `std::once_flag`-guarded
    `sodium_init()` so neither `main()` nor Catch2's `main()` has to remember
    to initialize it.
  - `backend/tests/unit/password_test.cpp` (new) + `backend/tests/CMakeLists.txt`.
- **Skill:** `backend-add-test` for the test wiring.
- **⚠️ Verify, don't assume:** the exact vcpkg port name and CMake target are
  believed to be `libsodium` / `find_package(unofficial-sodium CONFIG REQUIRED)`
  / `unofficial-sodium::sodium`, but **this is not certain** — check the
  installed port's own `*-config.cmake` (or `vcpkg search libsodium`) before
  writing it. If the port turns out to be awkward, the fallback is the
  standalone Argon2 reference library, at the cost of hand-plumbing salts.
- **Decide & record:** Argon2id over bcrypt (D1); `INTERACTIVE` cost preset
  and why not `MODERATE`/`SENSITIVE`; PRIVATE linkage rationale; the
  `once_flag` init approach.
- **Test shape:** hashing the same password twice gives **different** strings
  (salt is random) yet both verify; a wrong password fails; the hash string
  starts with `$argon2id$`; an empty/garbage stored hash fails verification
  instead of crashing. Keep the number of hash calls small — each one
  deliberately costs ~64 MiB and tens of ms (that's the point), so a loop of
  hundreds would make the fast tier slow.

**▶ Prompt**
```
Use the backend subagent to implement Step S1 of Phase 4.
Attached: PROJECT_PLAN.md, PHASE_4_USERS_AND_ACCESS_ARCHITECTURE.md,
PHASE_4_USERS_AND_ACCESS_IMPLEMENTATION.md. Recent history:
[GIT LOG HERE]

Goal: add password hashing as pure domain code. Add libsodium to
backend/vcpkg.json and CMakeLists.txt — IMPORTANT: verify the real vcpkg port
name and CMake target against the installed port instead of trusting the
names guessed in the docs, and tell me what you found. Link it PRIVATE on
atenciosamente_core and explain why PRIVATE is right here while libpqxx had
to be PUBLIC. Add src/domain/password.{hpp,cpp} with hash_password() and
verify_password() over crypto_pwhash_str/_str_verify using the INTERACTIVE
limits, with sodium_init() behind a std::once_flag so test binaries work too.
password.hpp must include only <string> — no <sodium.h> in the header.
Add tests/unit/password_test.cpp: same password hashed twice yields different
strings that both verify, wrong password fails, garbage stored hash fails
without crashing. Keep the hash-call count low — each call is intentionally
expensive.
Explain the Argon2id output format to me as you go (what's actually inside
that one string, and why there's no separate salt column).
When done: record the D1 decisions in PROJECT_PLAN.md §10 and commit in
`Scope (Tag): summary` style (no body, no trailers).
```

---

## S2 — Session token primitives (`domain/token`)

Generate unguessable tokens and hash them for storage. Still pure domain.

- **Files:** `backend/src/domain/token.{hpp,cpp}` (new) —
  `std::string generate_session_token();` (32 bytes from
  `randombytes_buf`, hex-encoded via `sodium_bin2hex` → 64 chars) and
  `std::string hash_token(const std::string& token);`
  (`crypto_hash_sha256`, hex-encoded). Add the `.cpp` to
  `atenciosamente_core`; add `backend/tests/unit/token_test.cpp`.
- **Skill:** `backend-add-test`.
- **Decide & record:** SHA-256 (fast, unsalted, deterministic) for tokens vs
  Argon2id (slow, salted) for passwords — and *why the asymmetry is correct*
  rather than inconsistent (D3: 256 bits of CSPRNG entropy isn't guessable,
  so there's nothing for a slow hash to buy, and an indexed
  `WHERE token_hash = $1` lookup *requires* determinism).
- **Test shape:** two generated tokens differ; length/charset is as expected;
  `hash_token` is stable across calls for the same input and differs for
  different inputs; `hash_token` output is never equal to its input.

**▶ Prompt**
```
Use the backend subagent to implement Step S2 of Phase 4.
Attached: PROJECT_PLAN.md + both Phase 4 docs. Recent history:
[GIT LOG HERE]

Goal: add src/domain/token.{hpp,cpp} with generate_session_token() (32 bytes
from libsodium's randombytes_buf, hex-encoded) and hash_token() (SHA-256 via
crypto_hash_sha256, hex-encoded). Wire the .cpp into atenciosamente_core and
add tests/unit/token_test.cpp (two tokens differ; hash is deterministic;
hash != input).
Explain why session tokens get a FAST unsalted hash while passwords got a
SLOW salted one — I want to understand why that asymmetry is deliberate and
not an inconsistency, including what would break if we salted the token hash.
When done: record the D3 decision in PROJECT_PLAN.md §10 and commit in
`Scope (Tag): summary` style (no body, no trailers).
```

---

## S3 — `users` table, `User` struct, user repository

First DB work of the phase. No HTTP yet.

- **Files:**
  - `backend/migrations/0002_create_users.sql` (new) — exactly the DDL in
    architecture §5.
  - `backend/src/domain/user.hpp` (new) — `User` (with `password_hash`) and
    `UserSummary` (**without** it) as two structs (§7).
  - `backend/src/domain/user_json.{hpp,cpp}` (new) — `to_json(const UserSummary&)`
    only, so a hash can't be serialized even by accident.
  - `backend/src/repository/user_repository.{hpp,cpp}` (new) — free functions
    over `pqxx::work&`: `insert(txn, email, password_hash)` (role hardcoded
    `'recipient'`, D6), `find_by_email(txn, email) -> std::optional<User>`,
    `find_by_id(txn, id) -> std::optional<UserSummary>`. `created_at` is read
    with the same `to_char(... AT TIME ZONE 'UTC', …)` expression
    `notification_repository.cpp` already uses — don't invent a second
    timestamp format.
  - `backend/CMakeLists.txt`, `backend/tests/CMakeLists.txt`,
    `backend/tests/integration/repository/user_repository_test.cpp` (new).
- **Skills:** `backend-add-migration`, `backend-add-test`.
- **Decide & record:** `role TEXT` + `CHECK` over a Postgres `ENUM` and over a
  separate roles table (D5); the `User`/`UserSummary` split as a structural
  (not remembered) guarantee; lowercase-in-C++ instead of the `citext`
  extension (§5).
- **Test shape:** insert then `find_by_email` round-trips; a second insert of
  the same email throws (the `UNIQUE` constraint — assert the failure, it's
  the behavior `409` will be built on); `find_by_email` on an unknown address
  returns `nullopt`; inserted role is `'recipient'`. Reuse the existing
  never-commit rollback isolation pattern verbatim.

**▶ Prompt**
```
Use the backend subagent to implement Step S3 of Phase 4.
Attached: PROJECT_PLAN.md + both Phase 4 docs. Recent history:
[GIT LOG HERE]

Goal: add the users table and its repository.
- migrations/0002_create_users.sql exactly as in architecture §5 (TEXT+CHECK
  for role, one password_hash column, email UNIQUE), matching 0001's style.
- src/domain/user.hpp with TWO structs: User (includes password_hash) and
  UserSummary (id/email/role only). src/domain/user_json.{hpp,cpp} with
  to_json() taking ONLY UserSummary.
- src/repository/user_repository.{hpp,cpp}: insert() (role hardcoded to
  'recipient', never taken from a caller), find_by_email() -> optional<User>,
  find_by_id() -> optional<UserSummary>. Free functions over pqxx::work&,
  pqxx::params for every value, and reuse notification_repository.cpp's
  existing to_char(...) expression for created_at rather than a new format.
- tests/integration/repository/user_repository_test.cpp following the
  existing never-commit rollback isolation: round-trip insert/find, duplicate
  email throws, unknown email returns nullopt, role is 'recipient'.
Run ./scripts/migrate.sh and the integration tier to confirm green.
Explain why the User/UserSummary split is a security property and not just
tidiness.
When done: record the D5 + User/UserSummary decisions in PROJECT_PLAN.md §10
and commit in `Scope (Tag): summary` style (no body, no trailers).
```

---

## S4 — `POST /users` (registration)

First endpoint. Proves S1 + S3 together.

- **Files:**
  - `backend/src/domain/register_user_request.{hpp,cpp}` (new) —
    `parse_register_user_request(const nlohmann::json&)` returning the same
    `ValidationResult`-shaped type as
    `create_notification_request.hpp`: object required, `email` and
    `password` must be non-empty strings, email shallow-validated and
    **lowercased**, password ≥ 8 chars, no composition rules (§8). It must
    **not** look at a `role` field at all (D6).
  - `backend/src/handlers/http_errors.{hpp,cpp}` (new) — move the
    `bad_request()` helper currently in `notifications.cpp`'s anonymous
    namespace here, and add `unauthorized()` (with
    `WWW-Authenticate: Bearer`) and `forbidden()`. Update
    `notifications.cpp` to use the shared version.
  - `backend/src/handlers/auth.{hpp,cpp}` (new) — `handle_register_user`:
    parse → `hash_password` → `user_repository::insert` → `txn.commit()` →
    `201`. Duplicate email must come back as `409`, not a 500: either check
    `find_by_email` first or catch the unique-violation
    (`pqxx::unique_violation`) — pick one and say why in the commit/decision.
  - `backend/src/app.cpp` — register the route;
    `backend/CMakeLists.txt` (new sources — note `auth.cpp` and
    `http_errors.cpp` go on `atenciosamente_server`, like
    `handlers/notifications.cpp`, because they include Crow);
    `backend/tests/unit/register_user_request_test.cpp` (new).
- **Skill:** `backend-add-endpoint`.
- **Decide & record:** role never read from the body (D6); `409` on duplicate
  and why that's deliberately *not* the generic treatment login gets (§8);
  registration returning a session is deferred to S5 (it needs
  `session_repository`) — until then `201` returns just the user.
- **Test shape (unit):** missing keys, wrong types, empty strings, short
  password → errors; mixed-case email comes back lowercased; a body
  containing `"role": "admin"` is accepted but the role is simply ignored —
  assert the parsed struct has no way to carry it.
- **Manual check:** `curl -X POST localhost:8080/users -d '{"email":"a@b.c","password":"hunter2hunter2"}'`
  → `201`; repeat → `409`; then `psql` and confirm the stored
  `password_hash` starts with `$argon2id$` and is nothing like the password.

**▶ Prompt**
```
Use the backend subagent to implement Step S4 of Phase 4.
Attached: PROJECT_PLAN.md + both Phase 4 docs. Recent history:
[GIT LOG HERE]

Goal: add POST /users (public registration).
- src/domain/register_user_request.{hpp,cpp} mirroring
  create_notification_request's ValidationResult pattern: require a JSON
  object with non-empty string email + password, lowercase the email, require
  password >= 8 chars with NO composition rules, shallow email validation
  (one '@', something either side, length cap — no RFC 5322 regex). It must
  NOT read a "role" field at all.
- src/handlers/http_errors.{hpp,cpp}: move bad_request() out of
  notifications.cpp's anonymous namespace into this shared file, add
  unauthorized() (with a WWW-Authenticate: Bearer header) and forbidden();
  update notifications.cpp to use them.
- src/handlers/auth.{hpp,cpp} with handle_register_user: validate, hash the
  password via domain/password.hpp, insert via user_repository, commit, return
  201 with the created UserSummary. A duplicate email must return 409 (not a
  500) — choose between a pre-check and catching pqxx's unique violation, and
  explain which you picked and why (think about the race between two
  simultaneous signups).
- Register the route in app.cpp; wire new sources in CMakeLists.txt; add
  tests/unit/register_user_request_test.cpp.
Then show me the curl commands to try it, and what the stored hash looks like
in psql.
When done: record the D6 decision + the 409-vs-generic reasoning in
PROJECT_PLAN.md §10 and commit in `Scope (Tag): summary` style.
```

---

## S5 — `sessions` table, login, logout, and `authenticate()`

The core of the phase: the guard every protected route will use.

- **Files:**
  - `backend/migrations/0003_create_sessions.sql` (new) — architecture §5.
  - `backend/src/domain/session.hpp` (new), `backend/src/domain/login_request.{hpp,cpp}`
    (new — same validation shape, lowercases email; no minimum length check
    on login, only on registration).
  - `backend/src/repository/session_repository.{hpp,cpp}` (new) —
    `insert(txn, user_id, token_hash, ttl_days)` writing `expires_at` as
    `now() + ($3 * INTERVAL '1 day')` (D9: policy constant in C++,
    arithmetic in SQL); `find_valid_by_token_hash(txn, token_hash)` doing the
    `JOIN users` with `AND expires_at > now()` and returning an
    `AuthenticatedUser`-ish value; `delete_by_token_hash(txn, token_hash)`.
  - `backend/src/handlers/auth.{hpp,cpp}` — add `handle_login`,
    `handle_logout`, and the shared
    `authenticate(pqxx::work&, const crow::request&) -> std::optional<AuthenticatedUser>`
    (reads the `Authorization` header, requires the `Bearer ` prefix, hashes,
    looks up). Login on unknown email must still run `verify_password`
    against a fixed dummy Argon2id hash before returning its generic `401`
    (§8 — the timing side channel). Make `handle_register_user` return a
    session too now, so register and login share one response shape.
  - `backend/src/app.cpp` — `POST /sessions`, `DELETE /sessions`.
  - Tests: `backend/tests/unit/login_request_test.cpp`,
    `backend/tests/integration/repository/session_repository_test.cpp`
    (expiry filtering is the interesting case).
- **Skills:** `backend-add-migration`, `backend-add-endpoint`, `backend-add-test`.
- **Decide & record:** DB-backed opaque tokens over JWT (D2); 30-day absolute
  expiry with no renewal (D9); the explicit-`authenticate()`-function shape
  over Crow middleware (D4); the dummy-hash timing defense; register
  auto-returning a session.
- **Test shape:** a session inserted with a *past* `expires_at` is **not**
  found by `find_valid_by_token_hash` (write that row directly in the test to
  force the case); a fresh one is; `delete_by_token_hash` makes it
  unfindable; an unknown hash returns `nullopt`.
- **Manual check:** login → copy the token → `curl -H "Authorization: Bearer
  <token>"` against a protected route (after S6); `DELETE /sessions` → the
  same token now `401`s. That last pair is the concrete thing JWT could not
  have done (D2).

**▶ Prompt**
```
Use the backend subagent to implement Step S5 of Phase 4.
Attached: PROJECT_PLAN.md + both Phase 4 docs. Recent history:
[GIT LOG HERE]

Goal: add sessions + login/logout + the authenticate() guard.
- migrations/0003_create_sessions.sql per architecture §5 (token_hash UNIQUE,
  user_id FK ON DELETE CASCADE, expires_at NOT NULL).
- src/domain/session.hpp and src/domain/login_request.{hpp,cpp} (lowercase
  the email; no password-length rule on login — that's a registration rule).
- src/repository/session_repository.{hpp,cpp}: insert() writing expires_at as
  now() + ($n * INTERVAL '1 day') with the 30-day TTL passed in from a named
  C++ constant, find_valid_by_token_hash() JOINing users and filtering
  expires_at > now(), delete_by_token_hash().
- src/handlers/auth.{hpp,cpp}: handle_login (generic 401 for both unknown
  email and wrong password, AND run verify_password against a fixed dummy
  argon2id hash on the unknown-email path so the timing matches),
  handle_logout (204, deletes the row), and the shared
  authenticate(pqxx::work&, const crow::request&) -> optional<AuthenticatedUser>
  that parses the "Authorization: Bearer <token>" header. Make
  handle_register_user return {token, user} too so register and login share
  one response shape.
- Routes POST /sessions and DELETE /sessions in app.cpp. Tests:
  tests/unit/login_request_test.cpp and
  tests/integration/repository/session_repository_test.cpp — include a row
  with a PAST expires_at and assert it is not found.
Explain the timing side-channel the dummy hash closes, and why we can revoke
a token instantly here when a JWT-based design couldn't.
When done: record D2, D4 and D9 in PROJECT_PLAN.md §10 and commit in
`Scope (Tag): summary` style.
```

---

## S6 — `/notifications` gains an owner and role checks

The breaking change. ⚠️ Includes the one destructive migration.

- **Files:**
  - `backend/migrations/0004_add_user_id_to_notifications.sql` (new) — the
    `DELETE FROM notifications;` + `ADD COLUMN user_id … NOT NULL` pair,
    **with the comment from architecture §5 explaining the deletion** kept
    verbatim in the file.
  - `backend/src/domain/notification.hpp` — add `user_id` and
    `recipient_email`; `notification_json.{hpp,cpp}` — serialize them;
    `create_notification_request.{hpp,cpp}` — add required
    `recipient_email` (shape validation only; resolving it to an id is the
    handler's job, since that needs the DB).
  - `backend/src/repository/notification_repository.{hpp,cpp}` — `insert`
    takes `user_id`; add `get_all_for_user(txn, user_id)`; both read queries
    `JOIN users` to populate `recipient_email` (D8: two named functions, not
    a flag).
  - `backend/src/handlers/notifications.{hpp,cpp}` — both handlers call
    `authenticate()` first (`401` if absent); `GET` branches on role
    (`get_all` vs `get_all_for_user`); `POST` requires
    `role == "admin"` (`403` otherwise), resolves `recipient_email` via
    `user_repository::find_by_email` (`400` if unknown).
  - `backend/src/app.cpp` — **the `GET /notifications` lambda must now take
    `const crow::request& req`**; today it takes no arguments, because it
    never needed the headers.
  - Update `backend/tests/unit/create_notification_request_test.cpp` and
    `backend/tests/integration/repository/notification_repository_test.cpp`
    (the latter now has to create a user first, and should assert that
    `get_all_for_user` doesn't return another user's rows).
- **Skills:** `backend-add-migration`, `backend-add-endpoint`, `backend-add-test`.
- **Decide & record:** the destructive `DELETE` over a nullable column or a
  backfill (§5) — log this one loudly; `401` vs `403` semantics;
  two-functions-not-a-flag (D8); leaking "no such recipient" to an admin
  being acceptable where the same leak on login would not be (D7).
- **Test shape:** create two users, insert one notification for each,
  and assert `get_all_for_user(a)` returns exactly a's; `get_all` returns
  both; `recipient_email` is populated.
- **Manual check:** recipient token on `POST /notifications` → `403`; admin
  token → `201`; no token → `401`; recipient `GET` shows only their own.

**▶ Prompt**
```
Use the backend subagent to implement Step S6 of Phase 4.
Attached: PROJECT_PLAN.md + both Phase 4 docs. Recent history:
[GIT LOG HERE]

Goal: make notifications per-user and role-gated.
WARNING: migrations/0004_add_user_id_to_notifications.sql deliberately runs
DELETE FROM notifications before adding a NOT NULL user_id column. Keep the
explanatory comment from architecture §5 in the file and tell me clearly
before I run it that it wipes my local notifications.
- domain/notification.hpp: add user_id + recipient_email; update
  notification_json and create_notification_request (recipient_email is
  required, shape-validated only).
- notification_repository: insert() takes user_id; add
  get_all_for_user(txn, user_id); both reads JOIN users for recipient_email.
  Two named functions, NOT one with an is_admin/optional flag.
- handlers/notifications.cpp: both handlers call authenticate() first and
  return 401 if it fails; GET picks get_all vs get_all_for_user by role; POST
  requires role=="admin" (403 otherwise) and resolves recipient_email via
  user_repository::find_by_email (400 "no such recipient" if missing).
- app.cpp: the GET /notifications lambda must now take const crow::request&
  so the handler can read the Authorization header.
- Update the affected unit + integration tests; the repository test should
  create two users and prove get_all_for_user never returns the other's rows.
Explain the 401-vs-403 distinction as you implement it, and why it's fine to
tell an admin "no such recipient" when login must never say "no such email".
When done: record the destructive-migration decision, the 401/403 semantics
and D8 in PROJECT_PLAN.md §10 and commit in `Scope (Tag): summary` style.
```

---

## S7 — Config, CI, and the first admin

Make the backend actually operable. Mostly verification, little new code.

- **Files:** `.env.example` (any new vars — e.g. the session TTL if it's made
  configurable; keep the "required → throw, tuning → default" split from
  `connection.cpp` vs `main.cpp`); `.github/workflows/backend-ci.yml`
  (confirm the new migrations apply in the integration job and the libsodium
  port builds on a cold cache — add apt build deps only if the port actually
  needs them); a short bootstrap note in the docs.
- **Skill:** none.
- **Decide & record:** the documented first-admin procedure (D6): register
  through the real endpoint, then
  `UPDATE users SET role = 'admin' WHERE email = '…';` via `psql` — and the
  decision *not* to build an `ADMIN_EMAILS` env var for a one-time event.
- **Check:** push and watch both CI jobs; cold-cache build is the risky part
  (new vcpkg port), so confirm it compiles without a warm binary cache.

**▶ Prompt**
```
Use the backend subagent to implement Step S7 of Phase 4.
Attached: PROJECT_PLAN.md + both Phase 4 docs. Recent history:
[GIT LOG HERE]

Goal: make Phase 4's backend operable and CI-green.
- Add any new env vars to .env.example, following the existing convention
  (required values throw like connection.cpp's read_env; tuning values get a
  default like main.cpp's read_pool_size). If the session TTL should be an env
  var rather than a C++ constant, argue for or against it rather than just
  doing it.
- Verify .github/workflows/backend-ci.yml: migrations 0002-0004 apply in the
  integration job, and the new libsodium dependency builds on a COLD vcpkg
  cache (add apt build dependencies only if the port genuinely needs them).
- Document the first-admin bootstrap: register normally through POST /users,
  then UPDATE users SET role='admin' WHERE email='hugortorquato@gmail.com';
  via psql. Explain why this is a documented manual step instead of an
  ADMIN_EMAILS env var.
Then walk me through a full manual smoke test with curl: register, login,
GET /notifications as a recipient, promote to admin, POST a notification to a
recipient, logout, and confirm the old token now 401s.
When done: record the bootstrap decision in PROJECT_PLAN.md §10 and commit in
`Scope (Tag): summary` style.
```

---

## S8 — Mobile: models, shared API config, auth client

First Flutter step. No UI yet — just the plumbing.

- **Files:**
  - `mobile/atenciosamente_app/lib/api/api_config.dart` (new) — move the
    `String.fromEnvironment('API_BASE_URL', …)` constant here out of
    `notifications_client.dart`, and add the `Authorization: Bearer` header
    builder so the header's spelling lives in exactly one place (§7).
  - `lib/models/user.dart`, `lib/models/session.dart` (new) — plain classes,
    `final` fields, `fromJson` factories, matching `notification.dart`'s
    hand-written style (no code generation).
  - `lib/api/auth_client.dart` (new) — top-level `register`, `login`,
    `logout` functions mirroring `notifications_client.dart`'s style
    (explicit status-code checks, `throw Exception(...)` with the backend's
    `{"error": …}` message when present).
  - `lib/api/notifications_client.dart` — import the shared config; add the
    `token` parameter (D10) and `recipientEmail` on create.
- **Skill:** `frontend-add-model`.
- **Decide & record:** `api_config.dart` as the first shared frontend module
  and why now (two API files = real duplication, matching the project's own
  "factor out when duplication appears" rule); token passed explicitly rather
  than read from a global (D10).

**▶ Prompt**
```
Use the frontend subagent to implement Step S8 of Phase 4.
Attached: PROJECT_PLAN.md + both Phase 4 docs. Recent history:
[GIT LOG HERE]

Goal: Flutter plumbing for auth, no UI yet.
- New lib/api/api_config.dart: move the API_BASE_URL String.fromEnvironment
  constant here from notifications_client.dart, plus a small helper that
  builds the {'Authorization': 'Bearer <token>', 'Content-Type':
  'application/json'} headers.
- New lib/models/user.dart and lib/models/session.dart: plain classes with
  final fields and fromJson factories, same hand-written style as
  models/notification.dart (no json_serializable).
- New lib/api/auth_client.dart: register(), login(), logout() mirroring
  notifications_client.dart's conventions exactly (explicit status-code
  checks, Exception with the backend's {"error":...} message when present).
- Update notifications_client.dart to use api_config.dart, take a token
  parameter on both functions, and take recipientEmail on create.
Explain, as someone coming from C++: why we're threading the token through as
an explicit parameter instead of a top-level mutable variable, and how that
mirrors the ConnectionPool-by-reference choice on the backend.
Run `flutter analyze` and confirm clean.
When done: record the api_config + explicit-token decisions in
PROJECT_PLAN.md §10 and commit in `Scope (Tag): summary` style.
```

---

## S9 — Mobile: login and register screens

- **Files:** `mobile/atenciosamente_app/pubspec.yaml` — add
  `flutter_secure_storage` (D11; verify the current version on pub.dev rather
  than guessing); `lib/screens/login_screen.dart`,
  `lib/screens/register_screen.dart` (new) — `StatefulWidget`s mirroring
  `create_notification_screen.dart`'s `Form` + `GlobalKey<FormState>` +
  `TextEditingController` + `_isSubmitting` pattern, `obscureText: true` on
  the password fields, each with a link to the other screen.
- **Skill:** `frontend-add-screen`.
- **Decide & record:** `flutter_secure_storage` over `shared_preferences`
  (D11 — a bearer token is a complete credential, and `shared_preferences` is
  a plaintext XML file); where the token actually gets written (a small
  storage helper used by both screens, not duplicated).
- **Check:** `flutter analyze`, then `./run_dev.sh` and register a real
  account from the phone against the running backend.

**▶ Prompt**
```
Use the frontend subagent to implement Step S9 of Phase 4.
Attached: PROJECT_PLAN.md + both Phase 4 docs. Recent history:
[GIT LOG HERE]

Goal: login and register screens.
- Add flutter_secure_storage to pubspec.yaml — check the current version on
  pub.dev instead of guessing one, and confirm the Android minSdk is
  satisfied. Explain why secure storage rather than shared_preferences for a
  bearer token.
- New lib/screens/login_screen.dart and register_screen.dart: StatefulWidgets
  following create_notification_screen.dart's exact pattern (Form +
  GlobalKey<FormState> + TextEditingControllers disposed in dispose() +
  _isSubmitting flag + SnackBar on error), obscureText on password fields,
  and a button to switch between the two screens.
- On success, persist the token with secure storage via one small shared
  helper used by both screens (don't duplicate the storage calls).
Keep it minimal and idiomatic — this app is deliberately the secondary
learning track. Run `flutter analyze`.
When done: record the D11 decision in PROJECT_PLAN.md §10 and commit in
`Scope (Tag): summary` style.
```

---

## S10 — Mobile: `AuthGate` + session wiring + logout

The step that makes the app coherent.

- **Files:**
  - `lib/screens/auth_gate_screen.dart` (new) — the new `MaterialApp.home`.
    Reads the stored token with a `FutureBuilder` (same idiom
    `NotificationsScreen` already uses), renders `LoginScreen` when absent and
    `NotificationsScreen(session: …)` when present. **No server round trip to
    validate the token** (§7) — a stale token surfaces as the first `401`.
  - `lib/main.dart` — `home: const AuthGate()`.
  - `lib/screens/notifications_screen.dart` — takes `Session`; passes the
    token to `fetchNotifications`; renders the create FAB only for
    `role == 'admin'` (UX only — the backend's `403` is the real control);
    adds a logout action that calls `DELETE /sessions`, clears storage, and
    returns to `LoginScreen`; handles a `401` from any call by bouncing to
    login.
  - `lib/screens/create_notification_screen.dart` — takes `Session`; adds the
    recipient-email field.
  - `lib/models/notification.dart` — add `recipientEmail`.
- **Skill:** `frontend-add-screen` for `AuthGate`.
- **Decide & record:** no named-route table and no routing package — `AuthGate`
  simply swaps `home`'s child (consistent with §9's non-goals); the
  role-conditional FAB being cosmetic, not a security boundary.
- **Check:** full manual run on the device: cold start → login screen;
  register → straight into the list; kill and reopen → still logged in;
  logout → back to login and the old token rejected; log in as the admin →
  FAB appears, create a notification for a recipient; log in as that
  recipient → it's there, and nobody else's are.

**▶ Prompt**
```
Use the frontend subagent to implement Step S10 of Phase 4.
Attached: PROJECT_PLAN.md + both Phase 4 docs. Recent history:
[GIT LOG HERE]

Goal: gate the app behind auth and wire the session through.
- New lib/screens/auth_gate_screen.dart as the app's home: a FutureBuilder
  reading the stored token (same idiom as NotificationsScreen's existing
  FutureBuilder) that shows LoginScreen when there's no token and
  NotificationsScreen(session: ...) when there is. Do NOT add a
  "validate my token" server call on startup — explain why the first 401 is a
  good enough signal.
- main.dart: home: const AuthGate().
- notifications_screen.dart: accept a Session, pass the token to the API
  calls, show the create FAB only when role == 'admin' (say in a comment that
  this is cosmetic and the backend's 403 is the real control), add a logout
  action (DELETE /sessions + clear storage + back to LoginScreen), and bounce
  to login on a 401 from any call.
- create_notification_screen.dart: accept the Session, add the recipient-email
  field. models/notification.dart: add recipientEmail.
No routing package and no state-management package — keep the imperative
Navigator style this app already uses.
Then give me a manual test script to run on the phone with ./run_dev.sh
covering: cold start, register, kill/reopen, logout, admin creates for a
recipient, recipient sees only their own.
When done: record the AuthGate + cosmetic-FAB decisions in PROJECT_PLAN.md
§10 and commit in `Scope (Tag): summary` style.
```

---

## S11 — Close the phase

- **Files:** `Documentation/phase-prompts/PHASE_4_USERS_AND_ACCESS_ARCHITECTURE.md`
  (tick §1's checklist), `Documentation/PROJECT_PLAN.md` (confirm every
  D1–D11 decision actually landed in §10 as it was implemented, and that §7's
  deployment ladder now notes that HTTPS/rung 3 is a prerequisite for leaving
  the LAN with real accounts).
- **Note:** the roadmap renumbering (§6 insert of Phase 4 after old Phase 3,
  old 4→5, 5+→6+) and `project_structure.md`'s stale "Phase 4 (push
  notifications)" reference were already done when these two documents were
  written — don't redo them, just verify.
- **Skill:** `organize-docs` as a final placement sanity check.

**▶ Prompt**
```
Close out Phase 4.
Attached: PROJECT_PLAN.md + both Phase 4 docs. Recent history:
[GIT LOG HERE]

Goal: verify and document, no new features.
- Tick the definition-of-done checklist in
  PHASE_4_USERS_AND_ACCESS_ARCHITECTURE.md §1, and tell me if anything on it
  did NOT actually get built.
- Check that every decision D1-D11 from the architecture doc has a
  corresponding row in PROJECT_PLAN.md §10, added when it was implemented.
  Add any that were missed, dated correctly.
- Add a line to PROJECT_PLAN.md §7 noting that rung 3 (reverse proxy for
  HTTPS) is now a prerequisite for using the app with real accounts off the
  LAN, since Phase 4 put passwords and bearer tokens on the wire.
- The §6 roadmap renumbering and project_structure.md's phase-number fix were
  already done — verify, don't redo.
Run the organize-docs skill as a final placement check, then commit in
`Scope (Tag): summary` style.
```

---

## Risk notes

Things most likely to bite, flagged up front:

| Risk | Where | Mitigation |
|---|---|---|
| vcpkg port/target name for libsodium isn't what the docs guess | S1 | Verify against the installed port *before* writing CMake; the standalone Argon2 lib is the fallback |
| Cold-cache CI build of a new native port fails or times out | S1, S7 | S7 explicitly checks a cold build; apt deps added only if needed |
| `DELETE FROM notifications` surprises you | S6 | Called out in the migration comment, the step, and the prompt |
| Argon2id's 64 MiB × concurrent logins in a 4-connection pool | S1, S5 | Known and accepted (§8); it's also why login is the natural DoS surface |
| Phase 1's `<chrono>` parsing gap resurfacing on `expires_at` | S5 | All expiry arithmetic/comparison stays in SQL (D9) |
| Two simultaneous signups racing on the same email | S4 | `UNIQUE` constraint is the real guard; the handler must turn that into `409`, not `500` |
