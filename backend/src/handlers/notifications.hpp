#pragma once

#include <crow.h>

#include "../db/connection_pool.hpp"

// Handler for GET /notifications.
// Acquires a connection from pool for the duration of the request and
// returns a JSON array of notifications read from Postgres.
crow::response handle_get_notifications(ConnectionPool& pool);

// Handler for POST /notifications.
// Parses req.body as JSON, expecting an object with non-empty string
// "title" and "body" fields. Returns 400 with a small JSON error body on
// any validation failure; on success, acquires a connection from pool,
// inserts the row, and returns 201 with the created Notification (id and
// created_at assigned by Postgres).
crow::response handle_post_notification(ConnectionPool& pool, const crow::request& req);
