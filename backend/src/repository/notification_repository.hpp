#pragma once

#include <pqxx/transaction>
#include <vector>

#include "../notification.hpp"

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
