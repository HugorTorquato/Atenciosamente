#include "notifications.hpp"

#include <pqxx/transaction>

#include "../db/connection.hpp"
#include "../notification_json.hpp"
#include "../repository/notification_repository.hpp"

crow::response handle_get_notifications() {
    // Note on errors, up front: make_connection() below, txn.exec() (inside
    // get_all()), and the timestamp parsing it does can all throw (missing
    // env var, Postgres unreachable, a malformed row). We deliberately don't
    // catch any of that here. Crow's router already wraps every handler call
    // in a try/catch (see routing.h's default_exception_handler): an
    // uncaught std::exception becomes an HTTP 500 with an empty body, and
    // the real message goes to the server's own log via CROW_LOG_ERROR — not
    // back to the client. That default is exactly what we want (don't leak
    // internals like DB connection strings or table names in a response
    // body), and app.cpp registers no custom exception-handling middleware
    // to be consistent with, so we lean on Crow's default rather than invent
    // a bespoke try/catch-per-handler pattern here.

    // Connection-per-request (see db/connection.hpp/.cpp for why): a fresh
    // socket + handshake for this one call, closed automatically by
    // pqxx::connection's destructor when `conn` goes out of scope at the end
    // of this function. No pool, no manual close — nothing here to leak.
    pqxx::connection conn = make_connection();

    // Every libpqxx query runs inside a transaction object. pqxx::work is
    // the "normal" read/write transaction type (there's also nontransaction
    // for read-only work with slightly less overhead, but work is the
    // conventional default and is what get_all() expects).
    //
    // We never call txn.commit() below, and that's deliberate, not a bug:
    // commit only matters when there are writes to make durable. get_all()
    // only runs a SELECT — there is nothing to persist and nothing to roll
    // back. When `txn` (and then `conn`) fall out of scope at the end of
    // this function, pqxx::work's destructor rolls back an uncommitted
    // transaction, which for a pure read is a no-op. This is also exactly
    // the mechanism S5's integration tests reuse deliberately: open a
    // transaction, do work, let it go out of scope uncommitted.
    pqxx::work txn{conn};

    const auto notifications = get_all(txn);

    crow::response res;
    res.code = 200;
    // Without this header, Crow defaults to text/plain.
    // Clients (browsers, curl --json, fetch()) use Content-Type to decide
    // how to parse the body — we must be explicit.
    res.set_header("Content-Type", "application/json");
    // .dump() converts the nlohmann::json value to a UTF-8 string.
    // No argument = compact (no extra whitespace). Pass an int for indentation:
    // .dump(2) gives pretty-printed output — useful when debugging by hand.
    res.body = serialize_notifications(notifications).dump();
    return res;
}
