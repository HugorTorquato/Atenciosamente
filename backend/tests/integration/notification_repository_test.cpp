#include <catch2/catch_test_macros.hpp>

#include "db/connection.hpp"
#include "repository/notification_repository.hpp"

#include <algorithm>
#include <pqxx/transaction>

// Integration tests: these hit a real Postgres (the Compose `db` service),
// unlike tests_unit which never touch a database. Each TEST_CASE opens its
// own connection and its own pqxx::work (a transaction), and — this is the
// isolation mechanism the whole file relies on — never calls txn.commit().
//
// pqxx::work's destructor checks whether the transaction was already
// committed (or aborted); if neither happened, it sends ROLLBACK over the
// connection before the socket closes. So when a TEST_CASE's txn goes out of
// scope at the end of the function (whether it ran to completion or a
// REQUIRE failed and unwound the stack), Postgres discards every write that
// happened inside it. No row from any test here ever becomes visible outside
// the transaction that created it, and nothing needs an explicit DELETE or
// TRUNCATE cleanup step. Running this file twice in a row should behave
// identically both times — that's the concrete thing to check when in doubt
// about whether rollback actually happened.
//
// This does mean these tests can't see each other's data and can't see rows
// left by a previous, separately-committed run (there shouldn't be any —
// nothing in this codebase commits notification rows outside of a real
// POST /notifications handler call). Each SECTION/TEST_CASE below builds
// whatever rows it needs inside its own transaction before asserting on them.

TEST_CASE("insert() then get_all() see the new row in the same transaction",
          "[notification][repository][integration]")
{
    pqxx::connection conn = make_connection();
    pqxx::work txn{conn};

    const Notification inserted =
        notification_repository::insert(txn, "Consulta agendada", "Sua consulta é amanhã às 10h");

    const std::vector<Notification> all = notification_repository::get_all(txn);
    const auto it = std::find_if(all.begin(), all.end(),
                                  [&](const Notification& n) { return n.id == inserted.id; });

    REQUIRE(it != all.end());
    REQUIRE(it->title == "Consulta agendada");
    REQUIRE(it->body == "Sua consulta é amanhã às 10h");

    // No txn.commit() here — see the file-level comment above. Rolled back
    // automatically when txn is destroyed at the end of this scope.
}

TEST_CASE("insert() returns a row with a populated id and created_at",
          "[notification][repository][integration]")
{
    pqxx::connection conn = make_connection();
    pqxx::work txn{conn};

    const Notification inserted = notification_repository::insert(txn, "Lembrete", "Beba água");

    // id is a server-generated IDENTITY column — it must be a real,
    // positive value assigned by Postgres, not left at some default.
    REQUIRE(inserted.id > 0);

    // created_at comes from the column's DEFAULT now(); it must not be the
    // default-constructed epoch time_point.
    REQUIRE(inserted.created_at != std::chrono::system_clock::time_point{});

    REQUIRE(inserted.title == "Lembrete");
    REQUIRE(inserted.body == "Beba água");
}

TEST_CASE("get_all() orders rows most-recent-first", "[notification][repository][integration]")
{
    pqxx::connection conn = make_connection();
    pqxx::work txn{conn};

    const Notification first = notification_repository::insert(txn, "Primeira", "Corpo 1");
    const Notification second = notification_repository::insert(txn, "Segunda", "Corpo 2");

    const std::vector<Notification> all = notification_repository::get_all(txn);

    // Find the positions of the two rows we just inserted; ORDER BY
    // created_at DESC, id DESC means the later insert (higher id) must
    // appear at or before the earlier one's position.
    const auto first_it =
        std::find_if(all.begin(), all.end(), [&](const Notification& n) { return n.id == first.id; });
    const auto second_it = std::find_if(all.begin(), all.end(),
                                         [&](const Notification& n) { return n.id == second.id; });

    REQUIRE(first_it != all.end());
    REQUIRE(second_it != all.end());
    REQUIRE(second_it < first_it);
}

TEST_CASE("insert() increases get_all()'s row count by exactly one",
          "[notification][repository][integration]")
{
    pqxx::connection conn = make_connection();
    pqxx::work txn{conn};

    // Baseline read first. Whatever real rows exist from committed
    // POST /notifications calls outside this test are none of this test's
    // business — it only cares about the *delta* one insert() produces
    // inside its own transaction, so it never needs to assume the table
    // starts empty (and never mutates rows it didn't create, unlike a
    // TRUNCATE would).
    const std::size_t before = notification_repository::get_all(txn).size();

    notification_repository::insert(txn, "Contador", "Deveria adicionar exatamente uma linha");

    const std::size_t after = notification_repository::get_all(txn).size();

    REQUIRE(after == before + 1);
}
