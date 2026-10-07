#include "app.hpp"

#include "handlers/notifications.hpp"

void setup_routes(crow::SimpleApp& app, ConnectionPool& pool) {
    CROW_ROUTE(app, "/")
    ([]() { return "hello"; });

    // Lambdas, not plain function names, because each handler now needs
    // pool — capturing it by reference forwards the same pool main()
    // constructed instead of each route getting (or owning) its own.
    CROW_ROUTE(app, "/notifications")
    ([&pool]() { return handle_get_notifications(pool); });

    // .methods(crow::HTTPMethod::POST) restricts this route to POST; the
    // same "/notifications" path already handles GET above via the
    // no-args overload.
    CROW_ROUTE(app, "/notifications")
        .methods(crow::HTTPMethod::POST)([&pool](const crow::request& req) {
            return handle_post_notification(pool, req);
        });
}
