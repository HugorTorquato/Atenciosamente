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

// Postgres's text representation of TIMESTAMPTZ depends on the server's
// DateStyle setting. The default ("ISO, MDY") renders something like
// "2026-07-26 14:30:00+00" — a space instead of a 'T', and an offset whose
// exact width (2 vs 4 digits, with or without a colon) isn't something we
// want to gamble on parsing correctly. Rather than trust the server's
// current formatting, we tell it exactly how to format the value in the
// query itself, via to_char(...). That turns "parse whatever Postgres feels
// like sending" into "parse one fixed, known string" — and we pick the same
// ISO-8601-with-Z shape notification_json.cpp's format_timestamp() already
// produces for the JSON response, so the two stay obviously in sync.
constexpr auto kCreatedAtSelectExpr =
    "to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"')";

// Parses the fixed "YYYY-MM-DDTHH:MM:SSZ" string kCreatedAtSelectExpr
// guarantees into a time_point.
//
// libpqxx (as of the 8.x we use here) only ships a chrono conversion for
// std::chrono::year_month_day (a plain date) — see pqxx/time.hxx. There is
// no built-in field::as<std::chrono::system_clock::time_point>(); we have
// to do this conversion by hand.
//
// The "obvious" C++20 tool here would be std::chrono::parse — the input
// counterpart to the std::format(chrono) call notification_json.cpp already
// uses for the other direction. It's not usable yet: libstdc++ (this
// container's GCC 13) implemented <chrono> *formatting* early but hasn't
// shipped chrono *parsing* — `std::chrono::parse` isn't defined, and the
// build fails with "'parse' is not a member of 'std::chrono'". So instead we
// extract the six fixed-width integers with sscanf (safe here specifically
// *because* the string's shape is guaranteed by kCreatedAtSelectExpr above —
// this is not a general-purpose datetime parser) and assemble a time_point
// out of plain C++20 chrono calendar types: year_month_day gives us
// midnight of that day as a sys_days, then we add the hour/minute/second
// offset as durations.
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

    // sys_days is a time_point on the system_clock scale, truncated to
    // whole days — exactly "midnight UTC of this calendar date". Adding
    // plain chrono durations on top gives us the full timestamp, and the
    // result converts implicitly to system_clock::time_point.
    return sys_days{ymd} + hours{hour} + minutes{minute} + seconds{second};
}

// Maps one result row into a Notification.
//
// txn.exec(...) returns a pqxx::result — an in-memory copy of the whole
// result set (libpqxx fetches eagerly unless you opt into txn.stream(...),
// which we don't need at Phase 1 row counts). Iterating a result with a
// range-for yields pqxx::row_ref — a lightweight, non-owning view into one
// row of that result (this libpqxx version's leaner alternative to the
// older, slightly heavier pqxx::row; both expose the same field access).
// row["colname"] returns a pqxx::field_ref — again a view, not a copy of
// the value — and field_ref.as<T>() parses the column's text wire value
// into T (throwing pqxx::conversion_error if the text doesn't convert).
// libpqxx ships as<T>() specializations for the plain numeric/string types
// below; TIMESTAMPTZ isn't one of them, so we pull it out as a string via
// as<std::string>() and hand it to parse_created_at() above.
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
    // The INSERT itself can only RETURNING the raw TIMESTAMPTZ, not the
    // to_char(...)-formatted string get_all() relies on above (RETURNING
    // clauses run before any further SELECT-level expressions could be
    // applied to them). Wrapping the INSERT in a CTE ("WITH inserted AS
    // (...)") lets us apply kCreatedAtSelectExpr to inserted.created_at in
    // an outer SELECT, so both query paths format the timestamp identically
    // and parse_created_at() below stays the single place that understands
    // that string shape.
    //
    // exec(sql, pqxx::params{txn, title, body}) is a *parameterized*
    // statement: title and body never get pasted into the SQL text as
    // characters. libpqxx sends the query string (with $1/$2 placeholders)
    // and the parameter values to Postgres as two separate pieces over the
    // wire; the server substitutes them at execution time, already knowing
    // each is meant to be a plain text value for a TEXT column. Contrast
    // that with building the string ourselves, e.g.
    //   "INSERT INTO notifications(title, body) VALUES ('" + title + "', ...)"
    // — if title were something like `', 'x'); DROP TABLE notifications; --`
    // that becomes part of the SQL *grammar*, not just its data, and runs as
    // additional statements/expressions. Parameterized queries sidestep that
    // whole class of bug: there is no step where user-supplied text is
    // parsed as SQL syntax, so there is nothing for an attacker's quotes,
    // semicolons, or comment markers to break out of. (This libpqxx version
    // spells that call `txn.exec(sql, pqxx::params{...})`; older libpqxx
    // used a now-deprecated `txn.exec_params(sql, ...)` shorthand for the
    // same thing — the safety property is identical either way.) Passing
    // `txn` as the first element of `params` isn't a third bind value — it
    // tells params which connection's text encoding to convert title/body
    // through; only title and body become $1/$2.
    const std::string sql =
        "WITH inserted AS ("
        "INSERT INTO notifications(title, body) VALUES ($1, $2) "
        "RETURNING id, created_at"
        ") "
        "SELECT id, " +
        std::string(kCreatedAtSelectExpr) + " AS created_at FROM inserted";
    const pqxx::result rows = txn.exec(sql, pqxx::params{txn, title, body});

    // one_row_ref() (rather than rows[0]) documents the assumption out
    // loud: a single-row INSERT ... RETURNING must produce exactly one row,
    // and it throws pqxx::unexpected_rows instead of silently reading
    // garbage if that assumption is ever wrong.
    const pqxx::row_ref row = rows.one_row_ref();

    // title/body are echoed back from the arguments rather than re-read
    // from the row: the CTE's outer SELECT only asks Postgres for id and
    // created_at (the two columns the caller couldn't have known ahead of
    // time), so there's nothing to re-parse for the other two fields.
    return Notification{
        row["id"].as<std::int64_t>(),
        title,
        body,
        parse_created_at(row["created_at"].as<std::string>()),
    };
}

}  // namespace notification_repository
