#pragma once
#include <crow.h>

#include "db/connection_pool.hpp"

// Registers all HTTP routes onto app. pool must outlive app (and every
// request app serves) — routes capture it by reference rather than owning
// it, so main() keeps the pool on its own stack frame.
// Defined in app.cpp; called by main and by tests.
void setup_routes(crow::SimpleApp& app, ConnectionPool& pool);
