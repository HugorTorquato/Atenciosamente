---
name: backend-add-migration
description: >-
  Add a SQL migration for the Atenciosamente Postgres database (e.g. create the
  notifications table). Use when introducing or changing DB schema. Covers the
  migration file convention and how it runs against the Compose `db` service.
---

# Add a database migration

The stack already runs **Postgres 16** as the Compose `db` service (see
`docker-compose.yml`); credentials come from `.env` (`POSTGRES_USER` / `_PASSWORD` /
`_DB`). Migrations are plain, ordered, forward-only SQL files checked into the repo.

> **First run note:** if no migration setup exists yet (Phase 1 is just starting), this is
> where it's established. Before writing files, confirm the chosen convention with the
> developer — it's an architectural decision, so record it in `PROJECT_PLAN.md`'s decision
> log. Default proposal below; adapt if a migration tool is later chosen.

## Convention (default)

- Folder: `backend/migrations/`.
- Filename: zero-padded, ordered, descriptive —
  `0001_create_notifications.sql`, `0002_...`. Numeric prefix defines apply order.
- Forward-only: never edit an applied migration; add a new one to change schema.
- Each file is idempotent where reasonable (`CREATE TABLE IF NOT EXISTS`).

The `notifications` columns must match the API/JSON shape already in the code
(`include/atenciosamente/notification.hpp` and `notification_json.hpp`): `id`, `title`,
`body`, `created_at`.

```sql
-- 0001_create_notifications.sql
CREATE TABLE IF NOT EXISTS notifications (
    id          BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    title       TEXT        NOT NULL,
    body        TEXT        NOT NULL,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);
```
*(Adjust the `id` definition to the agreed style; the point is id/title/body/created_at.)*

## Procedure

1. Create `backend/migrations/<NNNN>_<description>.sql` with the next free number.
2. Write the schema change; match column names/types to the C++ `Notification` struct and
   its JSON serialization so the persistence layer maps cleanly.
3. **Apply it** against the running `db` service to verify it's valid SQL:
   ```bash
   docker compose up -d db
   docker compose exec -T db psql -U "$POSTGRES_USER" -d "$POSTGRES_DB" \
     < backend/migrations/<NNNN>_<description>.sql
   ```
4. Verify: `docker compose exec db psql -U "$POSTGRES_USER" -d "$POSTGRES_DB" -c "\d notifications"`.
5. If you introduced the migration mechanism (how/when migrations auto-apply — entrypoint,
   init script, or app startup), document it in `PROJECT_PLAN.md` and the backend section of
   `Documentation/reference/project_structure.md`.
6. **Commit** — one line, `Backend (DB): add <NNNN> <description> migration` (no body/trailers).

Keep schema and the C++ model in lockstep — a mismatch surfaces as a runtime libpqxx error,
not a compile error.
