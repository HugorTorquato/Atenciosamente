#pragma once

#include <chrono>
#include <string>

struct Notification {
    int id;
    std::string title;
    std::string body;
    std::chrono::system_clock::time_point created_at;
};
