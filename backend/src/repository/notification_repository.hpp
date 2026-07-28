#pragma once

#include <pqxx/transaction>
#include <string>
#include <vector>

#include "../domain/notification.hpp"

// Reads every row from the notifications table, most recent first, and maps
// each one into a Notification.
//
// Takes an already-open pqxx::work& (a transaction) rather than a
// pqxx::connection: the caller opens the connection and starts the
// transaction, this function just runs a query inside it. That keeps the
// transaction boundary visible and owned by the caller instead of hidden
// inside this function — exactly what S4's INSERT (same transaction as a
// later read) and S5's tests (BEGIN before the test, ROLLBACK after) need to
// compose with. See notification_repository.cpp for the fuller reasoning on
// why this is a free function rather than a repository class.
//
// Read-only: never calls txn.commit(). Callers don't need to either — see
// handlers/notifications.cpp for why that's correct, not an oversight.
std::vector<Notification> get_all(pqxx::work& txn);

// Inserts one row and returns the fully-populated Notification: title/body
// are echoed back from the arguments (Postgres doesn't change them), id and
// created_at come from the database via an INSERT ... RETURNING clause —
// id because it's a server-generated IDENTITY column, created_at because
// its DEFAULT now() means the caller can't know it in advance.
//
// Takes pqxx::work& for the same reason get_all() does: the caller owns the
// transaction boundary. That matters here specifically because it lets the
// handler call insert() and, if a later step needed to, run more queries in
// the *same* transaction before a single commit() — and it's exactly what
// lets tests (S5) call insert() inside a transaction they roll back instead
// of commit, so test rows never actually land in the table.
//
// Uses txn.exec_params(...) — a parameterized statement, not string
// concatenation — so title/body are sent to Postgres as separate data
// values, never spliced into the SQL text. See notification_repository.cpp
// for why that's what makes this safe against SQL injection.
Notification insert(pqxx::work& txn, const std::string& title, const std::string& body);
