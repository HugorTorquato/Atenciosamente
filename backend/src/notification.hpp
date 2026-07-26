#pragma once

#include <chrono>
#include <cstdint>
#include <string>

struct Notification {
    std::int64_t id;
    std::string title;
    std::string body;
    std::chrono::system_clock::time_point created_at;
};
