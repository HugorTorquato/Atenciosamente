#include "app.hpp"
#include "db/connection_pool.hpp"

int main() {
    crow::SimpleApp app;
    ConnectionPool pool(4);
    setup_routes(app, pool);
    app.port(8080).multithreaded().run();
}
