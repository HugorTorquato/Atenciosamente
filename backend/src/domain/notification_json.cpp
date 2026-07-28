#include "notification_json.hpp"

#include <chrono>
#include <format>

namespace {

// Converts a time_point to an ISO 8601 UTC string: "2026-04-25T14:30:00Z".
//
// system_clock::time_point is always UTC by the C++20 standard — it measures
// duration elapsed since the Unix epoch (1970-01-01T00:00:00Z). There is no
// timezone ambiguity; the "Z" suffix is always correct here.
std::string format_timestamp(std::chrono::system_clock::time_point tp) {
    // floor<seconds> truncates sub-second precision.
    // Without this, %T would emit "14:30:00.000123456" — valid, but noisy.
    auto truncated = std::chrono::floor<std::chrono::seconds>(tp);

    // std::format with chrono types is a C++20 feature (<format> + <chrono>).
    // %F expands to %Y-%m-%d  →  "2026-04-25"
    // %T expands to %H:%M:%S  →  "14:30:00"
    // The literal "Z" is the ISO 8601 designator for UTC — NOT %Z, which
    // would emit the string "UTC" and produce non-standard output.
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
