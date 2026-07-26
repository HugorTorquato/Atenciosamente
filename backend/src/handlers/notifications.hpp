#pragma once

#include <crow.h>

// Handler for GET /notifications.
// Returns a JSON array of notifications read from Postgres.
crow::response handle_get_notifications();
