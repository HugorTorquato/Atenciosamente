#include "app.hpp"

int main() {
    crow::SimpleApp app;
    setup_routes(app);
    app.port(8080).multithreaded().run();
}
