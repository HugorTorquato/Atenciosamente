#include "create_notification_request.hpp"

#include <utility>

ValidationResult parse_create_notification_request(const nlohmann::json& body) {
    // A syntactically valid JSON document can still be the wrong *shape* —
    // a bare number, string, array, or null. We need a JSON object with
    // named fields, so reject anything else before calling body.at(...),
    // which would otherwise throw nlohmann::json::type_error.
    if (!body.is_object()) {
        return {std::nullopt, "request body must be a JSON object"};
    }
    if (!body.contains("title") || !body.contains("body")) {
        return {std::nullopt, "title and body are required"};
    }

    const nlohmann::json& title_value = body.at("title");
    const nlohmann::json& body_value = body.at("body");
    if (!title_value.is_string() || !body_value.is_string()) {
        return {std::nullopt, "title and body must be strings"};
    }

    std::string title = title_value.get<std::string>();
    std::string notification_body = body_value.get<std::string>();
    if (title.empty() || notification_body.empty()) {
        return {std::nullopt, "title and body must not be empty"};
    }

    return {CreateNotificationRequest{std::move(title), std::move(notification_body)}, ""};
}
