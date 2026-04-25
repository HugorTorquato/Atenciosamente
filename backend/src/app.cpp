#include "app.hpp"

void setup_routes(crow::SimpleApp& app)
{
    CROW_ROUTE(app, "/")
    ([]() { return "hello"; });
}
