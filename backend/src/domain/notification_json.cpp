#include "notification_json.hpp"

#include <chrono>
#include <format>

namespace {

// Converts a time_point to an ISO 8601 UTC string: "2026-04-25T14:30:00Z".
std::string format_timestamp(std::chrono::system_clock::time_point tp) {
    auto truncated = std::chrono::floor<std::chrono::seconds>(tp);
    return std::format("{:%FT%T}Z", truncated);
}

}  // namespace

nlohmann::json to_json(const Notification& n) {
    return {
        {"id", n.id},
        {"title", n.title},
        {"body", n.body},
        {"created_at", format_timestamp(n.created_at)},
    };
}

nlohmann::json serialize_notifications(std::span<const Notification> notifications) {
    nlohmann::json array = nlohmann::json::array();
    for (const auto& n : notifications) {
        array.push_back(to_json(n));
    }
    return array;
}
