#include <catch2/catch_test_macros.hpp>

#include "domain/create_notification_request.hpp"

TEST_CASE("parse_create_notification_request validates a POST /notifications body",
          "[notification][validation]")
{
    SECTION("valid object with title and body succeeds")
    {
        auto result = parse_create_notification_request({{"title", "Hello"}, {"body", "World"}});

        REQUIRE(result.request.has_value());
        REQUIRE(result.request->title == "Hello");
        REQUIRE(result.request->body == "World");
    }

    SECTION("a JSON array is rejected")
    {
        auto result = parse_create_notification_request(nlohmann::json::array({1, 2, 3}));

        REQUIRE_FALSE(result.request.has_value());
        REQUIRE(result.error == "request body must be a JSON object");
    }

    SECTION("a bare JSON string is rejected")
    {
        auto result = parse_create_notification_request("not an object");

        REQUIRE_FALSE(result.request.has_value());
        REQUIRE(result.error == "request body must be a JSON object");
    }

    SECTION("missing title is rejected")
    {
        auto result = parse_create_notification_request({{"body", "World"}});

        REQUIRE_FALSE(result.request.has_value());
        REQUIRE(result.error == "title and body are required");
    }

    SECTION("missing body is rejected")
    {
        auto result = parse_create_notification_request({{"title", "Hello"}});

        REQUIRE_FALSE(result.request.has_value());
        REQUIRE(result.error == "title and body are required");
    }

    SECTION("non-string title is rejected")
    {
        auto result = parse_create_notification_request({{"title", 42}, {"body", "World"}});

        REQUIRE_FALSE(result.request.has_value());
        REQUIRE(result.error == "title and body must be strings");
    }

    SECTION("non-string body is rejected")
    {
        auto result = parse_create_notification_request({{"title", "Hello"}, {"body", nullptr}});

        REQUIRE_FALSE(result.request.has_value());
        REQUIRE(result.error == "title and body must be strings");
    }

    SECTION("empty title is rejected")
    {
        auto result = parse_create_notification_request({{"title", ""}, {"body", "World"}});

        REQUIRE_FALSE(result.request.has_value());
        REQUIRE(result.error == "title and body must not be empty");
    }

    SECTION("empty body is rejected")
    {
        auto result = parse_create_notification_request({{"title", "Hello"}, {"body", ""}});

        REQUIRE_FALSE(result.request.has_value());
        REQUIRE(result.error == "title and body must not be empty");
    }
}
