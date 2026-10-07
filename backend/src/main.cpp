#include <cstdlib>
#include <string>

#include "app.hpp"
#include "db/connection_pool.hpp"

namespace {

constexpr std::size_t kDefaultPoolSize = 4;

std::size_t read_pool_size() {
    const char* value = std::getenv("POSTGRES_POOL_SIZE");
    if (value == nullptr || value[0] == '\0') {
        return kDefaultPoolSize;
    }
    return std::stoul(value);
}

}  // namespace

int main() {
    crow::SimpleApp app;
    ConnectionPool pool(read_pool_size());
    setup_routes(app, pool);
    app.port(8080).multithreaded().run();
}
