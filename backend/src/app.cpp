#include "app.hpp"

#include "handlers/notifications.hpp"

void setup_routes(crow::SimpleApp& app) {
    CROW_ROUTE(app, "/")
    ([]() { return "hello"; });

    // We pass the function by name — Crow accepts any callable that matches
    // the route's parameter signature. No lambda needed when the handler is
    // already a plain free function.
    CROW_ROUTE(app, "/notifications")(handle_get_notifications);

    // .methods(crow::HTTPMethod::POST) restricts this route to POST; the
    // same "/notifications" path already handles GET above via the
    // no-args overload (Crow dispatches on method, not just path). The
    // handler takes a `const crow::request&` (to read the body), so it's
    // registered with that signature instead of the no-arg one above.
    CROW_ROUTE(app, "/notifications").methods(crow::HTTPMethod::POST)(handle_post_notification);
}
