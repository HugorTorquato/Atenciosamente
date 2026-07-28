#include "notifications.hpp"

#include <nlohmann/json.hpp>
#include <pqxx/transaction>
#include <string>

#include "../db/connection.hpp"
#include "../domain/create_notification_request.hpp"
#include "../domain/notification_json.hpp"
#include "../repository/notification_repository.hpp"

namespace {

// Builds a minimal 400 response: a JSON object with one "error" key. Every
// validation failure in handle_post_notification() below returns through
// here, so the shape of a bad-request response is defined in exactly one
// place.
crow::response bad_request(const std::string& message) {
    crow::response res;
    res.code = 400;
    res.set_header("Content-Type", "application/json");
    res.body = nlohmann::json{{"error", message}}.dump();
    return res;
}

}  // namespace

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

    const auto notifications = notification_repository::get_all(txn);

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

crow::response handle_post_notification(const crow::request& req) {
    // nlohmann::json::parse's 3-argument overload: the middle nullptr is
    // "no SAX callback", and passing allow_exceptions = false turns a parse
    // failure (empty body, truncated JSON, not JSON at all) into a
    // *discarded* json value instead of a thrown parse_error — so we check
    // is_discarded() below instead of wrapping this in a try/catch. We use
    // nlohmann::json here (not crow::json::load) specifically so request
    // validation can be pure nlohmann::json logic, matching the response
    // side (notification_json.cpp) and factored into
    // create_notification_request.{hpp,cpp} where it's unit-testable
    // without a live server or database — see that header for the reasoning.
    const nlohmann::json parsed = nlohmann::json::parse(req.body, nullptr, false);
    if (parsed.is_discarded()) {
        return bad_request("request body must be valid JSON");
    }

    const ValidationResult validation = parse_create_notification_request(parsed);
    if (!validation.request) {
        return bad_request(validation.error);
    }

    // Same connection-per-request pattern as handle_get_notifications()
    // above: open, use, let RAII close it at scope exit.
    pqxx::connection conn = make_connection();
    pqxx::work txn{conn};

    // Unlike the GET handler, this one writes — insert() below runs an
    // INSERT ... RETURNING inside `txn`. The insert isn't durable until we
    // call commit(): if anything after insert() were to throw before we got
    // here, txn's destructor would roll the insert back instead of leaving
    // a half-finished request's data behind. Right now nothing sits between
    // insert() and commit() but that ordering (do the work, then commit)
    // is the pattern to keep as more steps get added to a handler.
    const Notification created =
        notification_repository::insert(txn, validation.request->title, validation.request->body);
    txn.commit();

    crow::response res;
    res.code = 201;
    res.set_header("Content-Type", "application/json");
    res.body = to_json(created).dump();
    return res;
}
