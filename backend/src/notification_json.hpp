#pragma once

#include "notification.hpp"

#include <nlohmann/json.hpp>
#include <span>

// Serializes a single Notification to a JSON object.
// Returns by value — the caller owns the result.
nlohmann::json to_json(const Notification& n);

// Serializes a collection of Notifications to a JSON array.
// std::span is a non-owning view: works with vector, array, or any contiguous range
// without copying the container or committing to a specific ownership model.
nlohmann::json serialize_notifications(std::span<const Notification> notifications);
