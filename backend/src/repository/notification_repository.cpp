#include "notification_repository.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <pqxx/params>
#include <pqxx/result>
#include <pqxx/row>
#include <stdexcept>
#include <string>

namespace {

constexpr auto kCreatedAtSelectExpr =
    "to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"')";

std::chrono::system_clock::time_point parse_created_at(const std::string& text) {
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    const int fields = std::sscanf(text.c_str(), "%d-%d-%dT%d:%d:%dZ", &year, &month, &day, &hour,
                                   &minute, &second);
    if (fields != 6) {
        throw std::runtime_error("notification_repository: could not parse created_at value '" +
                                 text + "'");
    }

    using namespace std::chrono;
    const year_month_day ymd{std::chrono::year{year}, std::chrono::month{unsigned(month)},
                             std::chrono::day{unsigned(day)}};
    if (!ymd.ok()) {
        throw std::runtime_error("notification_repository: invalid created_at date '" + text + "'");
    }

    return sys_days{ymd} + hours{hour} + minutes{minute} + seconds{second};
}

// Maps one result row into a Notification.
Notification row_to_notification(const pqxx::row_ref& row) {
    return Notification{
        row["id"].as<std::int64_t>(),
        row["title"].as<std::string>(),
        row["body"].as<std::string>(),
        parse_created_at(row["created_at"].as<std::string>()),
    };
}
}  // namespace

namespace notification_repository {

std::vector<Notification> get_all(pqxx::work& txn) {
    const pqxx::result rows =
        txn.exec("SELECT id, title, body, " + std::string(kCreatedAtSelectExpr) +
                 " AS created_at FROM notifications ORDER BY created_at DESC, id DESC");

    std::vector<Notification> notifications;
    notifications.reserve(rows.size());
    for (const auto& row : rows) {
        notifications.push_back(row_to_notification(row));
    }
    return notifications;
}

Notification insert(pqxx::work& txn, const std::string& title, const std::string& body) {
    const std::string sql =
        "WITH inserted AS ("
        "INSERT INTO notifications(title, body) VALUES ($1, $2) "
        "RETURNING id, created_at"
        ") "
        "SELECT id, " +
        std::string(kCreatedAtSelectExpr) + " AS created_at FROM inserted";
    const pqxx::result rows = txn.exec(sql, pqxx::params{txn, title, body});
    const pqxx::row_ref row = rows.one_row_ref();

    return Notification{
        row["id"].as<std::int64_t>(),
        title,
        body,
        parse_created_at(row["created_at"].as<std::string>()),
    };
}

}  // namespace notification_repository
