#pragma once
#include <crow.h>

// Registers all HTTP routes onto app.
// Defined in app.cpp; called by main and by tests.
void setup_routes(crow::SimpleApp& app);
