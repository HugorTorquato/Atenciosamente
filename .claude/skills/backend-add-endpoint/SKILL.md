---
name: backend-add-endpoint
description: >-
  Add a new HTTP endpoint to the Atenciosamente C++ (Crow) backend: a handler
  free function, its header declaration, registration in app.cpp, and a Catch2
  test. Use when adding any route (e.g. POST /notifications). Backend-only.
---

# Add a backend HTTP endpoint

One endpoint = one handler **free function** that takes the request it needs and returns a
`crow::response`, declared in a header, registered in `src/app.cpp`, and covered by a test.
Handlers live in `app.cpp` (not `main.cpp`) so the app is testable without running a server.

Reference the existing GET as the template: `src/handlers/notifications.{hpp,cpp}` and its
registration `CROW_ROUTE(app, "/notifications")(handle_get_notifications);` in `src/app.cpp`.

## Procedure

1. **Pick the file.** Group by resource — a notifications route goes in
   `src/handlers/notifications.cpp` with its declaration in `src/handlers/notifications.hpp`.
   New resource → new `handlers/<resource>.{hpp,cpp}` pair.
2. **Declare** the handler in the `.hpp` (include `<crow.h>`; document method + path in a
   comment). For routes with a body or params, take what you need, e.g.
   `crow::response handle_post_notification(const crow::request& req);`.
3. **Implement** in the `.cpp`. Set `res.code` and an explicit
   `res.set_header("Content-Type", "application/json");`. Reuse the JSON helpers in
   `include/atenciosamente/notification_json.hpp` rather than building JSON by hand. Parse
   request bodies with `crow::json::load(req.body)` and return `400` on invalid input.
4. **Register** the route in `src/app.cpp` inside `setup_routes`, with the HTTP method:
   `CROW_ROUTE(app, "/notifications").methods(crow::HTTPMethod::POST)(handle_post_notification);`
5. **Confirm the build wiring.** A new `.cpp` under `src/` must be part of the
   `atenciosamente_core` target — check `backend/CMakeLists.txt` and add it if the glob
   doesn't pick it up.
6. **Test** in `tests/unit/` (Catch2). Test the unit you can without a live server — the
   serialization/validation logic — following `notification_json_test.cpp`'s style. (Full
   request/response integration tests against Postgres come with `backend-add-migration`
   and Phase 1.)
7. **Build & run tests** in the dev container:
   `cmake --preset=dev && cmake --build --preset=dev && ctest --preset=dev`.
8. **Commit** — one line, `Backend (Feat): add <METHOD> <path>` (no body, no trailers).

Keep the heavy explanatory comment style of the existing handlers — this codebase is read
to learn from. Explain any new C++ concept (RAII, `std::optional`, parsing) in a comment.
