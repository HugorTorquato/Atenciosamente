#include "connection.hpp"

#include <cstdlib>
#include <format>
#include <stdexcept>
#include <string>

namespace {

// Reads an environment variable, throwing a clear error if it's missing or
// empty. We deliberately fail loudly here instead of substituting a
// default: a silently-wrong default (e.g. quietly connecting to
// "localhost" when POSTGRES_HOST was meant to be "db") is far harder to
// debug than a startup crash with an explicit "which variable, exactly"
// message.
std::string read_env(const char* name) {
    // std::getenv returns a raw non-owning pointer into the process
    // environment (or nullptr if unset) — copy it into a std::string
    // immediately so we're not holding a pointer whose lifetime rules are
    // fuzzier than "as long as this function needs it".
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        throw std::runtime_error(
            std::format("make_connection: missing or empty environment variable {}", name));
    }
    return value;
}

// libpqxx (via libpq) connection strings use "keyword=value" pairs. A value
// containing whitespace or a quote must be wrapped in single quotes, with
// any backslash or single quote *inside* it escaped with a backslash. Of
// our five values, only POSTGRES_PASSWORD is likely to contain something
// unusual in practice, but we apply the same escaping to all of them —
// one small function, no special-casing to get wrong.
std::string escape_conninfo_value(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char c : value) {
        if (c == '\\' || c == '\'') {
            escaped += '\\';
        }
        escaped += c;
    }
    return escaped;
}

}  // namespace

pqxx::connection make_connection() {
    const std::string conninfo =
        std::format("host='{}' port='{}' dbname='{}' user='{}' password='{}'",
                    escape_conninfo_value(read_env("POSTGRES_HOST")),
                    escape_conninfo_value(read_env("POSTGRES_PORT")),
                    escape_conninfo_value(read_env("POSTGRES_DB")),
                    escape_conninfo_value(read_env("POSTGRES_USER")),
                    escape_conninfo_value(read_env("POSTGRES_PASSWORD")));

    // pqxx::connection's constructor is what actually opens the socket and
    // runs the handshake — by the time this line finishes, the connection
    // is live. There's no separate "open()" call to remember, and nothing
    // for us to close later: that's handled by ~connection() automatically,
    // whenever this object (or wherever it ends up after moving) goes out
    // of scope.
    //
    // Returning it "by value" here doesn't copy anything — pqxx::connection
    // has no copy constructor (connections aren't things you duplicate),
    // only a move constructor. In practice we don't even pay for a move:
    // this is a single prvalue constructed directly in the return
    // statement, so C++17's mandatory copy elision applies and the caller's
    // variable *is* this connection, not a moved-from copy of it.
    return pqxx::connection(conninfo);
}
