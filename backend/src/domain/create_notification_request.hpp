#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>

// The validated fields needed to insert a new notification. By construction,
// if you have one of these, title and body are known-non-empty strings —
// every rule parse_create_notification_request() checks has already passed.
struct CreateNotificationRequest {
    std::string title;
    std::string body;
};

// The outcome of validating a POST /notifications body: exactly one of
// `request` or `error` is meaningful, mirroring the two things a handler
// needs to decide between — proceed to the repository, or answer 400.
// (A tagged std::variant/std::expected would express "exactly one" more
// strictly, but std::expected is C++23 and this project targets C++20; a
// plain struct with an optional keeps the same call-site clarity —
// `if (!result.request) return bad_request(result.error);` — without extra
// machinery.)
struct ValidationResult {
    std::optional<CreateNotificationRequest> request;
    std::string error;
};

// Validates an already-parsed JSON value against the POST /notifications
// contract:
//   - must be a JSON object
//   - must have "title" and "body" keys
//   - both values must be strings
//   - neither string may be empty
//
// Deliberately takes a plain nlohmann::json rather than a crow::request or
// crow::json::rvalue, and touches no database: it is pure data-in,
// data-out logic, which is what makes it unit-testable without a live
// server or a Postgres connection. handlers/notifications.cpp is the thin
// adapter that turns a real crow::request into the nlohmann::json this
// takes, and turns this function's result into an actual HTTP response.
ValidationResult parse_create_notification_request(const nlohmann::json& body);
