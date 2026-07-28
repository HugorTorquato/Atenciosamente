#pragma once

#include <crow.h>

// Handler for GET /notifications.
// Returns a JSON array of notifications read from Postgres.
crow::response handle_get_notifications();

// Handler for POST /notifications.
// Parses req.body as JSON, expecting an object with non-empty string
// "title" and "body" fields. Returns 400 with a small JSON error body on
// any validation failure; on success, inserts the row and returns 201 with
// the created Notification (id and created_at assigned by Postgres).
crow::response handle_post_notification(const crow::request& req);
