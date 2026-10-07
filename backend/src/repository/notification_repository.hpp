#pragma once

#include <pqxx/transaction>
#include <string>
#include <vector>

#include "../domain/notification.hpp"

namespace notification_repository {

// Reads every row from the notifications table, most recent first, and maps
// each one into a Notification.
std::vector<Notification> get_all(pqxx::work& txn);

// Inserts one row and returns the fully-populated Notification: title/body
Notification insert(pqxx::work& txn, const std::string& title, const std::string& body);

}  // namespace notification_repository
