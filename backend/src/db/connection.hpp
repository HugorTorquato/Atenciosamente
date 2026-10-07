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
// Opens one new connection per call — no pooling here. ConnectionPool
// (connection_pool.hpp) is what pools these for the request path; it calls
// this function once per slot at construction, so handlers should acquire
// a connection from a pool rather than calling this directly.
//
// Throws std::runtime_error if a required env var is missing or empty, or
// pqxx::broken_connection (itself derived from std::runtime_error) if
// Postgres refuses the connection (wrong password, DB not up, etc.).
pqxx::connection make_connection();
