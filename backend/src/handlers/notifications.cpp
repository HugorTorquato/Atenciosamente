#include "notifications.hpp"

#include <nlohmann/json.hpp>
#include <pqxx/transaction>
#include <string>

#include "../domain/create_notification_request.hpp"
#include "../domain/notification_json.hpp"
#include "../repository/notification_repository.hpp"

namespace {

// Builds a minimal 400 response: a JSON object with one "error" key. Every
// validation failure in handle_post_notification() below returns through
// here.
crow::response bad_request(const std::string& message) {
    crow::response res;
    res.code = 400;
    res.set_header("Content-Type", "application/json");
    res.body = nlohmann::json{{"error", message}}.dump();
    return res;
}

}  // namespace

crow::response handle_get_notifications(ConnectionPool& pool) {
    auto lease = pool.acquire();

    // We never call txn.commit() below, and that's deliberate, not a bug.
    pqxx::work txn{*lease};

    const auto notifications = notification_repository::get_all(txn);

    crow::response res;
    res.code = 200;
    res.set_header("Content-Type", "application/json");
    // .dump() converts the nlohmann::json value to a UTF-8 string.
    // No argument = compact (no extra whitespace). Pass an int for indentation:
    // .dump(2) gives pretty-printed output — useful when debugging by hand.
    res.body = serialize_notifications(notifications).dump();
    return res;
}

crow::response handle_post_notification(ConnectionPool& pool, const crow::request& req) {
    const nlohmann::json parsed = nlohmann::json::parse(req.body, nullptr, false);
    if (parsed.is_discarded()) {
        return bad_request("request body must be valid JSON");
    }

    const ValidationResult validation = parse_create_notification_request(parsed);
    if (!validation.request) {
        return bad_request(validation.error);
    }

    auto lease = pool.acquire();
    pqxx::work txn{*lease};

    const Notification created =
        notification_repository::insert(txn, validation.request->title, validation.request->body);
    txn.commit();

    crow::response res;
    res.code = 201;
    res.set_header("Content-Type", "application/json");
    res.body = to_json(created).dump();
    return res;
}
