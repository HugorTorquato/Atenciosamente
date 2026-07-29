#!/usr/bin/env bash
#
# Applies migrations/*.sql to the database in order, tracking what's already
# been applied in a schema_migrations table so re-running is a no-op.
#
# Connection: uses $DATABASE_URL if set (CI sets this directly, per
# contexts/github-actions.md's Phase 1 job); otherwise composes one from the
# POSTGRES_* vars docker-compose already passes into this container.
#
# Usage: scripts/migrate.sh   (also available as scripts/dev.sh migrate)
set -euo pipefail
cd "$(dirname "$0")/.."

if ! command -v psql >/dev/null 2>&1; then
    echo "psql not found — run this inside the backend dev container:" >&2
    echo "  docker compose exec backend bash" >&2
    exit 1
fi

DATABASE_URL="${DATABASE_URL:-postgresql://${POSTGRES_USER}:${POSTGRES_PASSWORD}@${POSTGRES_HOST}:${POSTGRES_PORT}/${POSTGRES_DB}}"

psql "$DATABASE_URL" -v ON_ERROR_STOP=1 -q -c "
CREATE TABLE IF NOT EXISTS schema_migrations (
    version     TEXT        PRIMARY KEY,
    applied_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);"

for file in migrations/*.sql; do
    version="$(basename "$file")"
    applied="$(psql "$DATABASE_URL" -tAc \
        "SELECT 1 FROM schema_migrations WHERE version = '$version'")"

    if [[ "$applied" == "1" ]]; then
        continue
    fi

    echo "==> Applying $version"
    psql "$DATABASE_URL" -v ON_ERROR_STOP=1 -q <<SQL
BEGIN;
\i $file
INSERT INTO schema_migrations (version) VALUES ('$version');
COMMIT;
SQL
done

echo "==> Database up to date"
