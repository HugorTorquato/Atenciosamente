#pragma once

#include <crow.h>

// Handler for GET /notifications.
// Returns a JSON array of hardcoded sample notifications.
crow::response handle_get_notifications();
