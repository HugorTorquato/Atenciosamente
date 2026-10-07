#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>

// The validated fields needed to insert a new notification.
struct CreateNotificationRequest {
    std::string title;
    std::string body;
};

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
ValidationResult parse_create_notification_request(const nlohmann::json& body);
