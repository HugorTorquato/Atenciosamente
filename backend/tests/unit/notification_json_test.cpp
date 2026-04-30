#include <catch2/catch_test_macros.hpp>

#include "notification_json.hpp"

#include <chrono>
#include <vector>

using namespace std::chrono;
using namespace std::chrono_literals;

// The Unix epoch is a convenient fixed timestamp: always produces
// "1970-01-01T00:00:00Z", which is easy to assert against.
static const system_clock::time_point epoch{};

TEST_CASE("serialize_notifications produces a JSON array", "[notification][json]")
{
    SECTION("empty vector produces empty array")
    {
        std::vector<Notification> notifications;
        auto result = serialize_notifications(notifications);

        REQUIRE(result.is_array());
        REQUIRE(result.empty());
    }

    SECTION("single notification has all required keys")
    {
        std::vector<Notification> notifications{
            {1, "Title", "Body", epoch}
        };
        auto result = serialize_notifications(notifications);

        REQUIRE(result.size() == 1);
        const auto& obj = result[0];
        REQUIRE(obj.contains("id"));
        REQUIRE(obj.contains("title"));
        REQUIRE(obj.contains("body"));
        REQUIRE(obj.contains("created_at"));
    }

    SECTION("single notification values are correct")
    {
        std::vector<Notification> notifications{
            {42, "Hello", "World", epoch}
        };
        auto result = serialize_notifications(notifications);
        const auto& obj = result[0];

        REQUIRE(obj["id"]         == 42);
        REQUIRE(obj["title"]      == "Hello");
        REQUIRE(obj["body"]       == "World");
        REQUIRE(obj["created_at"] == "1970-01-01T00:00:00Z");
    }

    SECTION("multiple notifications preserve order")
    {
        std::vector<Notification> notifications{
            {1, "First",  "Body 1", epoch},
            {2, "Second", "Body 2", epoch + 1h},
            {3, "Third",  "Body 3", epoch + 2h},
        };
        auto result = serialize_notifications(notifications);

        REQUIRE(result.size() == 3);
        REQUIRE(result[0]["id"] == 1);
        REQUIRE(result[1]["id"] == 2);
        REQUIRE(result[2]["id"] == 3);
    }

    SECTION("timestamp is formatted as ISO 8601 UTC")
    {
        auto tp = epoch + hours{1} + minutes{30} + seconds{45};
        std::vector<Notification> notifications{{1, "T", "B", tp}};

        REQUIRE(serialize_notifications(notifications)[0]["created_at"]
                == "1970-01-01T01:30:45Z");
    }
}
