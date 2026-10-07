#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <pqxx/connection>
#include <vector>

class ConnectionPool;

class ConnectionLease {
public:
    ConnectionLease(const ConnectionLease&) = delete;
    ConnectionLease& operator=(const ConnectionLease&) = delete;

    ConnectionLease(ConnectionLease&& other) noexcept;
    ConnectionLease& operator=(ConnectionLease&& other) noexcept;

    // Returns the connection to the pool (if this lease hasn't already been
    // moved from) and wakes one thread blocked in ConnectionPool::acquire().
    ~ConnectionLease();

    // Bare access to the underlying connection. No locking here: once a
    // slot is checked out, the pool guarantees no other thread touches it
    // (in_use_ is true) and the connections_ vector itself never resizes
    // after construction, so indexing it needs no synchronization.
    pqxx::connection& operator*() const;
    pqxx::connection* operator->() const;

private:
    // Only ConnectionPool may construct a lease — that's what "checking a
    // connection out" means, so only the pool's own bookkeeping (acquire())
    // is allowed to hand one out.
    friend class ConnectionPool;
    ConnectionLease(ConnectionPool* pool, std::size_t index);

    // pool_ doubles as the "has this lease already given its connection
    // back" flag: nullptr means moved-from (or already released), and both
    // the destructor and the move operations check it before touching the
    // pool. Without this, a moved-from lease's destructor would try to
    // release the same slot its new owner is still holding — a
    // double-release that could hand the same connection to two threads at
    // once, defeating the whole point of the pool.
    ConnectionPool* pool_;
    std::size_t index_;
};

// Owns a fixed number of already-open pqxx::connections, built once at
// construction via the existing make_connection() (one real TCP socket +
// Postgres handshake per slot, paid up front instead of per request).

// Deliberately pools N connections rather than guarding a single
// connection with a mutex. A pqxx::connection can only run one query at a
// time, so wrapping one in a mutex would serialize all DB access across
// every request — exactly the bottleneck Crow's multithreaded server is
// trying to avoid. Pooling N connections lets up to N requests touch
// Postgres truly concurrently; the mutex here only ever protects the tiny,
// fast bookkeeping of which slots are free, not the queries themselves.
class ConnectionPool {
public:
    explicit ConnectionPool(std::size_t size);

    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    ConnectionPool(ConnectionPool&&) = delete;
    ConnectionPool& operator=(ConnectionPool&&) = delete;

    // Blocks (does not error or spin) if every connection is currently
    // checked out, until another thread's lease is destroyed and frees one
    // up. Returns a move-only ConnectionLease that returns its connection
    // automatically when it goes out of scope.
    ConnectionLease acquire();

private:
    friend class ConnectionLease;
    void release(std::size_t index);

    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<pqxx::connection> connections_;
    std::vector<bool> in_use_;
};
