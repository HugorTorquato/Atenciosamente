# Phase 4 — Users & Access Control: Architecture

> The **design** document for Phase 4: what accounts, passwords, logins and
> roles look like in this project, and *why* each choice beats the obvious
> alternative. It describes the target state — it does not implement anything
> and contains no step-by-step work.
>
> Companion docs: the step breakdown lives in
> [`PHASE_4_USERS_AND_ACCESS_IMPLEMENTATION.md`](PHASE_4_USERS_AND_ACCESS_IMPLEMENTATION.md);
> the master decision log is [`PROJECT_PLAN.md`](../PROJECT_PLAN.md); the repo
> map is [`reference/project_structure.md`](../reference/project_structure.md).

---

## 1. Goal & definition of done

**Goal:** the backend stops treating notifications as one global anonymous
list and starts knowing *who* each notification is for, and *who is asking*.
Two kinds of people exist: an **admin**, who creates notifications aimed at
someone, and a **recipient**, who can only read the ones aimed at them.

Phase 4 is **done** when all of these are true:

- [ ] Anyone can create a `recipient` account with an email + password, and
      the password is stored as an Argon2id hash — never in plaintext, never
      reversible.
- [ ] Logging in returns a bearer token; the app stores it on the device in
      OS-backed secure storage and sends it on every subsequent request.
- [ ] `GET /notifications` requires a valid token and returns **only the
      caller's own** notifications when the caller is a recipient, and all of
      them when the caller is an admin.
- [ ] `POST /notifications` requires a valid token **and** `role = 'admin'`;
      it targets a recipient by email. A recipient calling it gets `403`.
- [ ] A request with no token, a garbage token, or an expired token gets
      `401` — and never leaks whether the email or the password was the wrong
      half on login.
- [ ] Logging out actually invalidates the token server-side (not just
      "the app forgot it").
- [ ] Unit tests cover hashing, token generation and request validation;
      integration tests cover the user/session repositories and the
      recipient-scoping of notification reads.
- [ ] The Flutter app opens on a login screen when there's no stored session,
      and on the notification list when there is.
- [ ] Every sub-task is committed, and each decision below is recorded in
      `PROJECT_PLAN.md` §10 as it actually gets implemented.

**What you'll learn:** how password storage actually works (and why "salt in
its own column" is a 2005 idea), the difference between *authentication* and
*authorization*, why session tokens and passwords get hashed with
*deliberately opposite* kinds of hash functions, how role checks are wired
without a framework doing it invisibly, and how a mobile client holds a
credential without becoming a global-variable soup.

---

## 2. Where we are → where Phase 4 lands

Today every request is anonymous and every notification is public:

```
Anyone on the LAN  ──GET /notifications──▶  handler ──▶ get_all(txn) ──▶ every row in the table
Anyone on the LAN  ──POST /notifications─▶  handler ──▶ insert(txn, title, body)
```

`backend/src/handlers/notifications.cpp` asks no questions. There is no
`users` table, no `Authorization` header handling, no `role` anywhere in the
codebase (the only `POSTGRES_USER`/`POSTGRES_PASSWORD` in the tree are
Postgres's *own* connection credentials, unrelated to app accounts).

After Phase 4:

```
                         ┌─ no / bad / expired token ──▶ 401
Request + Bearer token ──┤
                         └─ valid ──▶ AuthenticatedUser{id, email, role}
                                          │
                        ┌─────────────────┴─────────────────┐
                        │                                   │
              role = recipient                        role = admin
                        │                                   │
         GET  → get_all_for_user(txn, id)      GET  → get_all(txn)
         POST → 403 forbidden                  POST → insert(txn, user_id, …)
```

**What's already in place and gets reused unchanged:**

| Already there | Where | Reused for |
|---|---|---|
| `ConnectionPool` + RAII lease | `backend/src/db/connection_pool.{hpp,cpp}` | every new handler acquires its connection the same way |
| `ValidationResult{optional<T>, error}` pattern | `backend/src/domain/create_notification_request.hpp` | register/login body validation copies this shape |
| Repository-takes-`pqxx::work&` convention | `backend/src/repository/notification_repository.{hpp,cpp}` | user/session repositories follow it exactly |
| Idempotent migration runner + `schema_migrations` | `backend/scripts/migrate.sh` | migrations `0002`–`0004` just drop in |
| Per-test transaction rollback isolation | `backend/tests/integration/repository/…` | new repository tests reuse it verbatim |
| `Form` + `TextEditingController` + `_isSubmitting` screen pattern | `mobile/.../screens/create_notification_screen.dart` | login and register screens mirror it |

So Phase 4 is **mostly new files following existing shapes**, plus one
genuinely invasive change: the notifications endpoints gain an identity, and
their contract changes.

---

## 3. Concepts you'll meet (read once)

- **Authentication vs. authorization.** Authentication = "who are you?"
  (checking the token). Authorization = "are you allowed to do this?"
  (checking `role == 'admin'`). They are separate steps, they fail with
  *different* status codes (`401` vs `403`), and conflating them is one of
  the most common auth bugs.
- **A password hash is a one-way function with a cost knob.** You never store
  the password. You store the output of a function that is cheap to compute
  once (one login) and ruinously expensive to compute a billion times (an
  attacker with a stolen database guessing passwords).
- **A salt is a per-password random value** mixed into the hash so that two
  users with the same password get different hashes, and so an attacker can't
  precompute a lookup table ("rainbow table") that works against everyone at
  once. It is **not a secret** — it just has to be unique per password.
- **The PHC string format.** Modern hashers return one self-describing ASCII
  string that contains the algorithm, its version, its cost parameters, the
  salt *and* the hash:
  `$argon2id$v=19$m=65536,t=2,p=1$c29tZXNhbHQ$RdescudvJCsgt3ub+b+dWRWJTmaaJObG`
  Verification parses it back out. **This is why there is no `salt` column
  and no `iterations` column** — everything needed to re-verify is inside the
  one string. It also means you can raise the cost parameters later and old
  hashes keep verifying with their old parameters.
- **Bearer token.** An opaque secret string the client sends on every request
  in an `Authorization: Bearer <token>` header. "Bearer" literally means
  *whoever holds it, is it* — there's no second factor binding it to the
  device, which is why it must be stored carefully on the client and
  invalidatable on the server.
- **RBAC (role-based access control).** Permissions attach to a named role,
  the role attaches to the user. The alternative (permissions attached
  directly to each user) only pays off when roles stop being able to describe
  reality — far past where this project is.
- **Enumeration & timing side channels.** If "unknown email" and "wrong
  password" produce different responses — different message, different status,
  or even *measurably different response time* — an attacker can use your
  login endpoint to discover which email addresses have accounts. Both halves
  have to look identical from the outside.

---

## 4. Decisions, and why the runner-up loses

Every decision below gets logged in `PROJECT_PLAN.md` §10 as it's actually
implemented. The format is deliberate: the alternative is named, not
strawmanned, and there's a line saying what would make us switch.

### D1 — Password hashing: **Argon2id**, not bcrypt

**Chosen:** Argon2id (via libsodium's `crypto_pwhash_str` /
`crypto_pwhash_str_verify`), with libsodium's `INTERACTIVE` cost preset
(~64 MiB memory, a few tens of milliseconds per hash).

**Runner-up:** bcrypt — still the most widely deployed password hash, battle
tested since 1999, available everywhere.

**Why Argon2id wins here:**

1. **Memory hardness.** bcrypt's cost knob only buys CPU time; it uses ~4 KiB
   of memory regardless of cost. An attacker with a GPU (thousands of weak
   cores, limited memory per core) gets a large parallelism advantage.
   Argon2id's cost knob buys *memory* — at 64 MiB per hash, a GPU with 24 GB
   can only run a few hundred guesses in parallel instead of tens of
   thousands. The defender pays this cost once per login; the attacker pays
   it per guess.
2. **No input length cap.** bcrypt silently truncates input at 72 bytes —
   a passphrase longer than that has its tail ignored, which is a
   *correctness* bug that is invisible until someone uses a long passphrase.
   Argon2id has no such limit.
3. **It's what you'd meet in a new codebase today.** Argon2id won the 2015
   Password Hashing Competition and is OWASP's first recommendation. Since
   the explicit goal is learning the current real-world pattern, learning the
   current recommendation beats learning the 1999 one.

**What would change our mind:** needing to verify a pre-existing corpus of
bcrypt hashes (migration scenario) — not our case, we have zero users.

**Explicitly rejected, for the record:** `SHA-256(password + salt)`, or any
hand-rolled "hash it a few times" scheme. General-purpose hashes are designed
to be *fast*, which is precisely the wrong property: a modern GPU does
billions of SHA-256 per second. The lesson worth internalizing is that
password hashing is the one place where you *want* the primitive to be slow,
and that you never roll the construction yourself.

### D2 — Session mechanism: **DB-backed opaque bearer token**, not JWT

**Chosen:** on login, generate 32 cryptographically random bytes, hex-encode
them, hand that string to the client as its token, and store only a
**SHA-256 hash of it** in a `sessions` table alongside `user_id` and
`expires_at`. Validating a request = hash the presented token, one indexed
`SELECT`, check `expires_at > now()`.

**Runner-up:** JWT — a signed, self-contained token carrying `user_id` and
`role` in its payload, verified with an HMAC key and no database lookup.

**Why the DB-backed token wins here:**

1. **Revocation is a real feature, and JWT doesn't have it.** Logout must
   actually invalidate the credential. With a session row, logout is
   `DELETE FROM sessions WHERE token_hash = …` — instant and total. With a
   JWT, a stolen token stays valid until it expires, no matter what the
   server wants, *unless* you add a server-side revocation list — at which
   point you're doing a database lookup per request anyway and have kept
   JWT's complexity for none of its benefit.
2. **No new secret to manage.** JWT introduces a signing key: a new required
   env var, a rotation story, and a family of well-known footguns
   (`alg: none`, HMAC-vs-RSA confusion, forgetting to validate `exp`).
   An opaque random token has no signature to get wrong — its security is
   just "256 bits of entropy, and the server looks it up."
3. **It doesn't violate the project's "stateless server" principle.**
   `PROJECT_PLAN.md` §4.1's statelessness is about the *process*: config from
   env, logs to stdout, nothing important in process memory, kill and restart
   freely. Sessions in Postgres are exactly as "stateless" as notifications in
   Postgres — the server still holds nothing. JWT's real selling point is
   avoiding the *database round trip*, which is a scaling optimization for
   traffic this project does not have (and the lookup is a primary-key-ish
   indexed hit on a tiny table).
4. **It fits the codebase's existing shape.** No signing library, no key
   plumbing, one more repository of exactly the kind that already exists
   twice.

**What would change our mind:** multiple independent services needing to
validate the same token without sharing a database — the actual problem JWT
is good at. Not our architecture.

### D3 — Hash tokens with **SHA-256**, hash passwords with **Argon2id** — deliberately different

This looks inconsistent and isn't. It's worth understanding *why* the two
secrets get opposite treatment:

| | Password | Session token |
|---|---|---|
| Where it comes from | A human brain | `randombytes_buf()` — a CSPRNG |
| Entropy | Low (maybe 20–40 bits, realistically) | 256 bits |
| Guessable? | Yes — that's the entire threat | No. Not in the lifetime of the universe |
| So the hash must be… | **Slow + salted**, to make each guess expensive | **Fast + deterministic**, so `WHERE token_hash = $1` is one indexed lookup |
| Chosen | Argon2id | SHA-256, unsalted |

Salting the token would also break the lookup: a salted hash is different
every time, so you couldn't find the row by hashing the presented token —
you'd have to fetch every session and verify one by one.

**Why hash the token at all, rather than storing it raw?** So a leaked
database dump isn't a bag of live credentials. Same reasoning GitHub uses for
personal access tokens. The cost is one SHA-256 per request — microseconds.

### D4 — Authorization check: **an explicit free function**, not Crow middleware

**Chosen:** a free function in the handler layer —

```cpp
std::optional<AuthenticatedUser> authenticate(pqxx::work& txn, const crow::request& req);
```

— called as the first thing inside each protected handler, with the role
check written out plainly right after it:

```cpp
auto lease = pool.acquire();
pqxx::work txn{*lease};

const auto user = authenticate(txn, req);
if (!user)                      return unauthorized();
if (user->role != "admin")      return forbidden();
```

**Runner-up:** Crow's middleware system (`CROW_MIDDLEWARES`, a struct with
`before_handle`/`after_handle` and a per-request `context`), which would run
the token check before the handler is ever entered and stash the user in the
request context.

**Why the explicit function wins here:**

1. **There is no middleware anywhere in this codebase.** Introducing a
   framework subsystem for its first use, in the same phase that introduces
   auth itself, means debugging two new things at once. The free function is
   the same shape as everything already in `handlers/` — and this project has
   consistently chosen explicit-at-the-call-site over invisible
   (`pqxx::work` opened by the handler, not by the repository;
   `ConnectionPool&` passed by parameter, not a global).
2. **Middleware's weakness is the failure mode that matters.** With
   middleware, protection comes from *remembering to list* the middleware on
   each route; a new route added without it is silently public, and nothing
   in the handler's body hints that it should have been protected. With the
   explicit call, an unprotected handler is visibly missing a line of code
   that every sibling handler has — it shows up in review and in a diff.
3. **Testability.** `authenticate(txn, req)` is callable from an integration
   test with a hand-built `crow::request`, no server and no middleware chain.

**What would change our mind:** a dozen protected routes, where repeating
three lines per handler turns into a real maintenance cost and the
"forgot to list the middleware" risk inverts. At two protected routes, the
explicit version is strictly clearer. A good halfway step, if it ever gets
noisy, is a small helper returning either a user or a ready-made error
response — still an explicit call, less repetition.

### D5 — Roles: **a `role` TEXT column with a CHECK constraint**

**Chosen:** `role TEXT NOT NULL CHECK (role IN ('admin', 'recipient'))` on
`users`.

**Runner-ups:** (a) a native Postgres `ENUM` type; (b) a separate `roles`
table with a join table for many-to-many.

**Why TEXT + CHECK wins here:**

1. **vs. ENUM:** adding a value to a Postgres enum needs `ALTER TYPE … ADD
   VALUE`, which has historically had awkward transaction semantics and makes
   the migration runner's "every file runs inside `BEGIN … COMMIT`" loop a
   special case. Removing or renaming a value is worse. A CHECK constraint is
   dropped and recreated with plain DDL inside a normal transaction. The
   database-side type safety you gain is the same either way — both reject
   `'wizard'`.
2. **vs. a roles table:** a join table is the right answer when a user can
   hold several roles, or when roles are created at runtime by users. Neither
   is true here, and probably never will be: this is "admin or not." A join
   table would add two tables and a JOIN to every single authorization check
   to model a boolean.
3. **It stays consistent with the existing C++ side**, where every column
   round-trips as a plain `std::string` (`title`, `body`). No enum mapping
   layer, no "what if the DB has a value the C++ enum doesn't know" branch.

**What would change our mind:** real per-feature permissions
(`can_create_notifications`, `can_delete_users`, …) assigned in combinations
— i.e. when "role" stops being able to describe who may do what. Then a
roles/permissions table earns its keep.

### D6 — Registration creates **only** recipients; the first admin is made by hand

**Chosen:** `POST /users` never reads a `role` field from the request body.
The insert hardcodes `'recipient'`. The first admin is created by registering
normally and then running one `UPDATE users SET role = 'admin' WHERE email =
'…';` in `psql`.

**Runner-up:** an `ADMIN_EMAILS` env var that auto-promotes matching emails at
registration, or an admin-only `POST /users` that can set a role.

**Why this wins here:**

1. **The attack it prevents is the one that matters.** With public
   self-registration (your decision), the single most dangerous bug would be
   a request body like `{"email":…,"password":…,"role":"admin"}` being
   honored. Never parsing the field at all is a stronger guarantee than
   parsing it and validating it — there is no code path to get it wrong.
2. **A one-time setup action doesn't deserve a feature.** Promoting the first
   admin happens exactly once, by the person who already has `psql` access to
   the database (that's how `scripts/migrate.sh` works too). An
   `ADMIN_EMAILS` env var would be a permanent piece of config, read on every
   registration, to automate a single event.
3. The password still goes through the real Argon2id path, because the
   account is created through the real endpoint — no hand-computed hash
   pasted into SQL.

**What would change our mind:** needing to create admins routinely (a team),
which would justify an admin-only user-management endpoint.

### D7 — Targeting a recipient: **by email, in the create body**

**Chosen:** `POST /notifications` takes `{"title", "body", "recipient_email"}`.
The handler resolves the email to a `user_id` with
`user_repository::find_by_email` and returns `400 {"error": "no such
recipient"}` if it misses.

**Runner-up:** take a numeric `user_id` directly, and add a `GET /users`
endpoint plus a picker UI so the admin can find the id.

**Why email wins here:**

1. **It's the identifier a human actually has.** An admin knows the person's
   email; nobody knows that they're user 47. Taking a `user_id` *forces* the
   `GET /users` endpoint and a picker screen into this phase just to make the
   endpoint usable.
2. **It defers a real design question instead of guessing.** A user-listing
   endpoint needs pagination, a search filter, and a decision about what an
   admin may see about other accounts. None of that is thought through yet,
   and a notification-targeting feature shouldn't be the thing that forces it.
3. **Leaking "that address has no account" here is acceptable**, which is the
   one objection worth taking seriously. The caller is an authenticated
   admin, not an anonymous attacker — unlike on login, where the same leak
   would be an enumeration oracle. Worth writing down explicitly so the
   difference between the two cases is a deliberate choice rather than an
   inconsistency.

**What would change our mind:** admins managing enough recipients that typing
the address is error-prone → then `GET /users` + a picker, with the resolved
id sent instead.

### D8 — Reads: **two named repository functions**, not one with a flag

**Chosen:**

```cpp
std::vector<Notification> get_all(pqxx::work& txn);                        // admin
std::vector<Notification> get_all_for_user(pqxx::work& txn, long user_id); // recipient
```

**Runner-up:** `get_all(txn, std::optional<long> user_id)` or
`get_all(txn, bool is_admin)` — one function, one branch inside.

**Why two functions win here:** the call site reads as what it is. A boolean
parameter at a call site (`get_all(txn, true)`) tells the reader nothing, and
the `is_admin` variant puts an *authorization* concept inside the
*persistence* layer — the repository would start deciding policy. With two
functions, the repository only knows how to run two different queries, and
the handler (which is where the role already lives) picks one. This matches
the existing split between `get_all` and `insert`: the repository exposes
capabilities, not decisions.

### D9 — Session lifetime: **absolute 30-day expiry**, computed in SQL from a C++ constant

**Chosen:** `expires_at` is written at insert time as
`now() + ($2 * INTERVAL '1 day')`, where the `30` comes from a named C++
constant passed as a bound parameter. Validation filters
`WHERE expires_at > now()`. No renewal, no sliding window, no refresh token.

**Runner-up (a):** short-lived access token + refresh token rotation, the
pattern most "proper" auth systems use.
**Runner-up (b):** compute the timestamp in C++ and send it as a formatted
string.

**Why this wins here:**

1. **vs. refresh tokens:** the access/refresh split exists to limit the
   damage window of a *leaked, unrevocable* access token — which is a JWT
   problem. With DB-backed tokens we can revoke instantly (D2), so the split
   buys very little and costs a second token type, a rotation endpoint, and
   replay-detection questions.
2. **vs. computing the timestamp in C++:** Phase 1 already hit the libstdc++
   gap where `<chrono>` *formats* but doesn't *parse*
   (`PROJECT_PLAN.md` §10, 2026‑07‑26), which is why `created_at` is
   formatted by Postgres with `to_char(...)`. Doing timestamp arithmetic in
   SQL keeps all timestamp handling on the database side, where it already
   is, and avoids reintroducing a timezone/format seam. The *policy* (how
   long a session lasts) still lives visibly in C++ as a named constant; only
   the arithmetic is delegated.
3. 30 days suits a phone app you don't want to re-login into weekly, at a
   threat level of "one developer on a LAN."

**What would change our mind:** real users with real data → shorter expiry,
and probably "log out all devices."

### D10 — Mobile: the session is **passed as a parameter**, not held in a global

**Chosen:** `AuthGate` loads the token from secure storage and passes a
`Session` object down: `NotificationsScreen(session: …)` →
`CreateNotificationScreen(session: …)`, and every `api/` function takes the
token as an explicit argument (`fetchNotifications(token)`).

**Runner-ups:** (a) a top-level mutable `Session? currentSession` in a Dart
file everything imports; (b) adopting Riverpod/Provider now.

**Why explicit parameters win here:**

1. **Symmetry with the backend's own rule.** This codebase has no globals or
   singletons anywhere, on purpose: `ConnectionPool` is built on `main()`'s
   stack and threaded through by reference, with a §10 entry explaining why.
   A mutable global on the Dart side would be the same mistake the C++ side
   deliberately avoided — and the project's whole premise is learning the
   discipline, not just getting pixels on screen.
2. **Every function's dependencies stay in its signature.** You can tell what
   needs a session by looking, and a widget test can construct one without
   any ambient setup.
3. **vs. Riverpod/Provider:** `PROJECT_PLAN.md` §9 explicitly defers state
   management "until we feel pain without it." Passing one object down two
   levels of widget tree is not pain. Adopting a state-management package
   here means learning its mental model *at the same time* as learning auth —
   and the mobile layer is deliberately the secondary learning track.

**What would change our mind:** the session needing to be read five levels
deep, or several screens needing to *mutate* shared state. That's the real
signal for `InheritedWidget`/Riverpod — and the honest first step would be
Flutter's own `InheritedWidget`, not a package.

### D11 — Token storage on device: **`flutter_secure_storage`**, not `shared_preferences`

**Chosen:** `flutter_secure_storage`, which on Android keeps the value in
`EncryptedSharedPreferences` with a key held in the hardware-backed Android
Keystore.

**Runner-up:** `shared_preferences` — simpler, no native dependency, already
the default answer for "save a small value."

**Why secure storage wins here:** `shared_preferences` writes a plaintext XML
file in the app's data directory. On a rooted device, via an ADB backup, or
through any process that gets filesystem access, a stored bearer token is
readable — and a bearer token is a *complete* credential (D-concept above:
whoever holds it, is it). The cost of the upgrade is one dependency and an
`async` read; the thing being protected is a 30-day login. This is also the
honest version of the real-world pattern — production apps do not keep
session tokens in plaintext preferences.

**What would change our mind:** nothing at this threat level; this is simply
the correct default for credentials.

---

## 5. Schema

Three migrations, following the existing `NNNN_description.sql` convention and
`0001`'s exact style (uppercase keywords, aligned types,
`BIGINT GENERATED ALWAYS AS IDENTITY`, `TIMESTAMPTZ … DEFAULT now()`).

```sql
-- 0002_create_users.sql
CREATE TABLE IF NOT EXISTS users (
    id             BIGINT      GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    email          TEXT        NOT NULL UNIQUE,
    password_hash  TEXT        NOT NULL,
    role           TEXT        NOT NULL CHECK (role IN ('admin', 'recipient')),
    created_at     TIMESTAMPTZ NOT NULL DEFAULT now()
);
```

- **One column for the whole hash** — see §3 (PHC string format). No `salt`
  column, no `iterations` column, no `algorithm` column.
- **`email` is `UNIQUE`**, which is what makes "email already registered"
  detectable (and is the integrity guarantee behind the `409` response).
- **Case handling:** emails are lowercased in C++ during validation, before
  any query, so plain `TEXT UNIQUE` behaves case-insensitively in practice.
  The alternative — Postgres's `citext` extension — would be this project's
  first extension dependency to solve a problem that one `std::tolower` pass
  solves.

```sql
-- 0003_create_sessions.sql
CREATE TABLE IF NOT EXISTS sessions (
    id          BIGINT      GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    user_id     BIGINT      NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    token_hash  TEXT        NOT NULL UNIQUE,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
    expires_at  TIMESTAMPTZ NOT NULL
);
```

- **`token_hash`, never the token** (D3). `UNIQUE` both enforces that and
  gives the index that every authenticated request hits.
- **`ON DELETE CASCADE`**: deleting a user takes their sessions with them.
  There's no account-deletion feature yet, so this is cheap insurance against
  orphaned rows rather than a considered retention policy.

```sql
-- 0004_add_user_id_to_notifications.sql
-- Rows created before Phase 4 have no owning user. This is still a local-only
-- dev database (PROJECT_PLAN.md §7, rung 1 — no deployment exists yet), so we
-- drop them rather than invent an owner for them.
--
-- The alternative — a NULLABLE user_id — would push a permanent "and what does
-- NULL mean here?" branch into every recipient-scoped query, forever, to
-- preserve throwaway test rows. If this ever has to run against data worth
-- keeping, replace the DELETE with a real backfill to a chosen user.
DELETE FROM notifications;

ALTER TABLE notifications
    ADD COLUMN user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE;
```

> ⚠️ This is the one destructive step in the phase. It wipes the local
> `notifications` table. That's a deliberate, documented call — flag it when
> implementing, don't let it be a surprise.

---

## 6. Endpoint contracts

| Method | Path | Auth required | Body | Success | Failures |
|---|---|---|---|---|---|
| `POST` | `/users` | — | `{"email","password"}` | `201 {"token","user":{"id","email","role"}}` | `400` invalid body · `409` email taken |
| `POST` | `/sessions` | — | `{"email","password"}` | `201 {"token","user":{…}}` | `400` invalid body · `401` generic |
| `DELETE` | `/sessions` | Bearer | — | `204` no content | `401` |
| `GET` | `/notifications` | Bearer | — | `200 [ {notification}, … ]` | `401` |
| `POST` | `/notifications` | Bearer + `role=admin` | `{"title","body","recipient_email"}` | `201 {notification}` | `400` invalid body / unknown recipient · `401` · `403` wrong role |

Notes that are themselves decisions:

- **Resource-shaped paths** (`/users`, `/sessions`) rather than
  `/auth/register` and `/auth/login`: creating a session *is* what logging in
  means, and deleting one *is* logging out, so `DELETE /sessions` comes for
  free instead of needing a `/auth/logout` verb-path. It's also the shape the
  existing `/notifications` route already follows.
- **Register returns a session too.** There's no email verification gate
  (§8), so making the app immediately log in after signing up avoids a
  pointless second round trip — and the response shape is identical to
  login's, so the client has one code path.
- **`401` vs `403`, used precisely.** `401` = we don't know who you are (no,
  malformed, or expired token). `403` = we know exactly who you are and
  you're not allowed. A `401` also gets a `WWW-Authenticate: Bearer` header,
  which is what the status code is specified to mean.
- **Every notification JSON gains `recipient_email`** (via a JOIN in the
  repository query). For a recipient it's just their own address echoed back;
  for an admin it's the "whose is this?" context that makes a single unscoped
  list readable without any user-listing feature.
- **Login's error is deliberately uninformative**: the same `401` and the
  same `{"error": "invalid email or password"}` whether the email is unknown
  or the password is wrong. See §8.

### The two flows, end to end

```
LOGIN
  app                      backend                                     postgres
   │  POST /sessions          │                                            │
   │  {email, password}       │                                            │
   ├─────────────────────────▶│  parse + lowercase email                   │
   │                          │  find_by_email ───────────────────────────▶│
   │                          │◀──────────────────── User{…, password_hash}│
   │                          │  verify_password(plain, hash)   ← Argon2id │
   │                          │  generate 32 random bytes → token          │
   │                          │  sessions.insert(sha256(token), +30d) ────▶│
   │◀─────────────────────────┤  201 {token, user}                         │
   │  store token in                                                       │
   │  flutter_secure_storage                                               │

AUTHENTICATED REQUEST
   │  GET /notifications      │                                            │
   │  Authorization: Bearer … │                                            │
   ├─────────────────────────▶│  authenticate(txn, req):                   │
   │                          │    read header → sha256 → lookup ─────────▶│
   │                          │◀──── session JOIN user (expires_at > now())│
   │                          │  no row?  → 401                            │
   │                          │  role == recipient → get_all_for_user ────▶│
   │                          │  role == admin     → get_all ─────────────▶│
   │◀─────────────────────────┤  200 [ … ]                                 │
```

---

## 7. Where the code goes

### Backend — following the existing four layers

```
backend/src/
├── domain/                        pure logic: no Crow, no pqxx, no I/O
│   ├── password.{hpp,cpp}         hash_password / verify_password     ← new
│   ├── token.{hpp,cpp}            generate_session_token / hash_token ← new
│   ├── user.hpp                   User + UserSummary structs          ← new
│   ├── user_json.{hpp,cpp}        to_json(UserSummary)                ← new
│   ├── session.hpp                Session struct                      ← new
│   ├── register_user_request.{hpp,cpp}   body validation              ← new
│   ├── login_request.{hpp,cpp}           body validation              ← new
│   ├── notification.hpp           + user_id, + recipient_email        ← changed
│   ├── notification_json.{hpp,cpp}       serialize the new fields     ← changed
│   └── create_notification_request.{hpp,cpp} + recipient_email        ← changed
├── repository/                    SQL against an already-open pqxx::work&
│   ├── user_repository.{hpp,cpp}      insert / find_by_email / find_by_id   ← new
│   ├── session_repository.{hpp,cpp}   insert / find_valid_by_token_hash /
│   │                                  delete_by_token_hash                   ← new
│   └── notification_repository.{hpp,cpp} + get_all_for_user, insert(…,user_id) ← changed
├── handlers/                      Crow adapters
│   ├── auth.{hpp,cpp}             handle_register / handle_login /
│   │                              handle_logout + authenticate()      ← new
│   ├── http_errors.{hpp,cpp}      bad_request / unauthorized / forbidden ← new
│   └── notifications.{hpp,cpp}    authenticate + role branch          ← changed
└── app.cpp                        register 3 new routes               ← changed
```

Two structural points worth calling out:

- **`password.cpp` belongs in `domain/`, even though it links a C library.**
  The `domain/` rule is "no Crow, no pqxx, no I/O" — not "no dependencies"
  (`notification_json.cpp` already depends on nlohmann/json). Hashing a
  string is a pure function of its input. Critically, `password.hpp` includes
  only `<string>` — `<sodium.h>` appears only in the `.cpp`. By this repo's
  own documented rule (`CMakeLists.txt:26-40`), that means libsodium links
  **PRIVATE** on `atenciosamente_core`, the mirror image of why libpqxx had
  to be PUBLIC.
- **`User` and `UserSummary` are two structs, not one.** `User` has
  `password_hash` (the login path needs it); `UserSummary` has only
  `id`/`email`/`role`, and `to_json` accepts *only* `UserSummary`. Leaking a
  password hash into an HTTP response therefore isn't "something to remember
  not to do" — it's not expressible. This is the "dumb data" principle
  (§4.3) doing security work.
- **`libsodium` needs `sodium_init()` once before first use**, and it is not
  safe to call concurrently. Rather than requiring `main()` to remember —
  which would also break the test binaries, that have Catch2's `main()` —
  `password.cpp` guards it with a `std::once_flag` internally. Callers can't
  get the ordering wrong because there's no ordering to get wrong.

### Mobile — following the existing folder conventions

```
mobile/atenciosamente_app/lib/
├── api/
│   ├── api_config.dart            base URL + auth header builder     ← new
│   ├── auth_client.dart           register / login / logout          ← new
│   └── notifications_client.dart  + token parameter, + recipientEmail ← changed
├── models/
│   ├── user.dart                  id / email / role                  ← new
│   ├── session.dart               token + User                       ← new
│   └── notification.dart          + recipientEmail                   ← changed
├── screens/
│   ├── auth_gate_screen.dart      the new MaterialApp home           ← new
│   ├── login_screen.dart                                             ← new
│   ├── register_screen.dart                                          ← new
│   ├── notifications_screen.dart  takes session; admin-only FAB;
│   │                              logout action                      ← changed
│   └── create_notification_screen.dart  takes session; + recipient field ← changed
└── main.dart                      home: const AuthGate()             ← changed
```

- **`api_config.dart` exists because a second API file now exists.** The base
  URL is currently a file-private `_baseUrl` constant inside
  `notifications_client.dart`; duplicating that `String.fromEnvironment` block
  into `auth_client.dart` is exactly the duplication the project's own
  "`widgets/` is added when duplication appears" convention says to factor
  out. It also hosts the one-line `Authorization: Bearer` header builder, so
  the header's spelling lives in one place.
- **`AuthGate` does not verify the token with the server on startup.** It just
  checks whether a token exists in secure storage and renders accordingly. If
  the token turns out to be stale, the first real request returns `401` and
  the app bounces to login. Adding a dedicated "is my token still good?"
  endpoint to make the splash screen more certain would be a round trip on
  every cold start to learn something the next request tells us anyway.
- **The admin-only "+" button is UX, not security.** Hiding it is a courtesy
  so recipients don't see a button that would fail; the actual guarantee is
  the backend's `403`. Worth saying out loud, because "the UI hides it" is a
  classic fake authorization control.

---

## 8. Security notes, stated honestly

- **🔴 Everything travels in plaintext HTTP today.** This phase introduces
  passwords on the wire and a bearer token on *every* request, while
  `PROJECT_PLAN.md` §7 has us on deployment rung 1: `docker compose up` on a
  laptop, phone talking to it over the LAN, no TLS. Anyone on that network can
  read both. This is an accepted limitation *at this rung*, not an oversight,
  and it is the single thing that must close before this app is used with
  anyone's real data: deployment rung 3 (reverse proxy for HTTPS) is now a
  prerequisite for leaving the LAN, not just a nice-to-have.
- **Login must not reveal which half was wrong** — identical status, identical
  message. And identical *timing*: when the email doesn't exist, the handler
  still runs `verify_password` against a fixed dummy Argon2id hash before
  returning, so "unknown email" doesn't answer measurably faster than "wrong
  password." Without that, the timing difference *is* the enumeration oracle
  the generic message was meant to close.
- **Registration's `409` is intentionally not generic**, which is a real
  (small) enumeration leak accepted on purpose: every signup form on the
  internet tells you your email is already registered, because the
  alternative — silently pretending to succeed — needs an email channel to
  tell the real owner, and we have none. Documented so a later reviewer
  doesn't "fix" one of these two rules to match the other.
- **No rate limiting. This is the biggest live gap**, and it pairs with the
  public-self-registration decision: nothing stops thousands of login guesses
  or thousands of junk accounts. Today's only mitigations are Argon2id's
  deliberate per-attempt cost (which also means a flood of login attempts is
  a CPU/memory DoS vector — worth knowing) and the generic error. The honest
  first fix is at the reverse-proxy rung, where a limiter is configuration
  rather than code.
- **Password rules: minimum length only** (8 characters), no
  uppercase/digit/symbol requirements. Per NIST SP 800-63B, composition rules
  push people toward predictable mutations (`Password1!`) without adding real
  entropy; length plus a slow hash is what actually helps. No maximum length
  either — Argon2id has no input cap (unlike bcrypt, D1).
- **Never log secrets.** Before any logging work happens in a later phase:
  request bodies for `POST /users` / `POST /sessions` and the `Authorization`
  header must never reach stdout.
- **Email validation stays deliberately shallow** — non-empty, exactly one
  `@` with something on both sides, a length cap. A "real" RFC 5322 regex is
  a famous trap (it accepts things no mail server does and rejects valid
  addresses), and genuine validation means "can we deliver to it," which
  needs mail infrastructure we don't have.
- **SQL injection** is already handled by the existing `pqxx::params`
  discipline — every new query keeps it. Emails and tokens are *never*
  concatenated into SQL text.
- **CORS** is not applicable: the client is a mobile app, not a browser.

---

## 9. Explicit non-goals for Phase 4

Each is deferred with a reason, in the spirit of the roadmap's own "Phase 0
explicit non-goals" list:

| Not doing | Why that's fine now |
|---|---|
| Password reset / "forgot password" | Needs an email channel that doesn't exist. Admin can reset by hand in `psql` until it does. |
| Email verification | No abuse pressure at single-developer/LAN scale; adding it means adding mail infra. |
| 2FA / TOTP | Guards against targeted account takeover — not a threat at this user count. |
| Refresh tokens / sliding expiry | Solves JWT's unrevocability, which D2 avoided entirely. |
| Admin user-management screen | No `GET /users` this phase (D7); first admin is a one-time `psql` command (D6). |
| Rate limiting | Named as the biggest gap (§8); belongs at the reverse-proxy rung. |
| Deleting expired session rows | `expires_at > now()` already makes them inert; physical cleanup is a future one-line migration. |
| "Log out of all devices" / session list | One phone per person is enough UX for now. |
| HTTPS/TLS | Deployment rung 3, not this phase (§8). |
| iOS | Project is Android-only so far; the Keychain path in `flutter_secure_storage` is irrelevant until it isn't. |

---

## 10. What this changes about Phases 4 and 5

Worth knowing while designing, because it's the reason this phase jumped the
queue:

- **Phase 4 (scheduled notifications)** gets a real question to answer:
  "whose notification is due?" A scheduler over an ownerless global list would
  have had to be rewritten the moment users appeared.
- **Phase 5 (push notifications)** needs device tokens, and a device token is
  meaningless without a user to attach it to — it'll be a `device_tokens`
  table with a `user_id`, which only exists after this phase.

---

Once the architecture above is agreed, the work itself is broken into steps in
[`PHASE_4_USERS_AND_ACCESS_IMPLEMENTATION.md`](PHASE_4_USERS_AND_ACCESS_IMPLEMENTATION.md).
