#pragma once

#include <pqxx/connection>

// Builds a connection string from the POSTGRES_* environment variables
// (POSTGRES_HOST, POSTGRES_PORT, POSTGRES_DB, POSTGRES_USER,
// POSTGRES_PASSWORD — see .env.example / docker-compose.yml) and returns an
// already-OPEN connection.
//
// RAII: pqxx::connection's destructor closes the socket to Postgres. There
// is no close_connection() to call anywhere in this codebase — whenever the
// returned object's scope ends (this function's caller returns, a handler's
// stack unwinds, etc.), the connection closes itself. See connection.cpp
// for the fuller explanation.
//
// Phase 1 deliberately opens one connection per call (i.e. one per request,
// once a handler calls this). Pooling is a Phase 2 concern — see the TODO
// in connection.cpp.
//
// Throws std::runtime_error if a required env var is missing or empty, or
// pqxx::broken_connection (itself derived from std::runtime_error) if
// Postgres refuses the connection (wrong password, DB not up, etc.).
pqxx::connection make_connection();
