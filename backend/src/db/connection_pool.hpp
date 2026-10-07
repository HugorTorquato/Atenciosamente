#pragma once

#include <pqxx/connection>

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <vector>

class ConnectionPool;

// A move-only RAII handle for one pqxx::connection checked out of a
// ConnectionPool. Mirrors the same idea as pqxx::work's auto-rollback
// (see repository/ for that), applied to a pooled resource instead of a
// transaction: construction removes a connection from the pool's available
// set, destruction puts it back. There is no manual "release()" anywhere in
// this class's public API on purpose — the only way a connection returns to
// the pool is this object's destructor running, so there is no code path
// where a caller can forget to give one back.
//
// Deliberately exposes only the bare pqxx::connection& (via operator*/->),
// not a pqxx::work. Phase 1 already decided (PROJECT_PLAN.md §10,
// 2026-07-26) that the transaction boundary stays explicit at the call
// site — repository functions take a pqxx::work&, not a connection, and the
// handler is what opens/commits it. Baking transaction creation into the
// lease would blur that boundary for no benefit: a caller that wants to run
// several statements in one transaction, or none at all (a plain read),
// still just does `pqxx::work txn{*lease};` itself, same as it did with
// make_connection() before this pool existed.
//
// Move-only, not copyable: a pqxx::connection itself can't be duplicated
// (it's a single live socket + session), so neither can a handle that
// claims exclusive ownership of one slot in the pool.
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
// Safe for multiple threads to call acquire() on concurrently: a
// std::mutex protects the in_use_ bookkeeping (which slots are currently
// checked out), and a std::condition_variable lets a thread block when
// every slot is busy instead of spin-polling for a free one.
//
// Deliberately pools N connections rather than guarding a single
// connection with a mutex. A pqxx::connection can only run one query at a
// time, so wrapping one in a mutex would serialize all DB access across
// every request — exactly the bottleneck Crow's multithreaded server is
// trying to avoid. Pooling N connections lets up to N requests touch
// Postgres truly concurrently; the mutex here only ever protects the tiny,
// fast bookkeeping of which slots are free, not the queries themselves.
class ConnectionPool {
public:
    // Opens `size` connections immediately (calls make_connection() `size`
    // times) and blocks until all of them succeed or one throws. There is
    // no lazy/on-demand opening — by the time this constructor returns,
    // every slot in the pool is a live, ready-to-use connection.
    explicit ConnectionPool(std::size_t size);

    // Not copyable or movable: std::mutex and std::condition_variable are
    // themselves neither, and a pool's whole point is to be one shared
    // instance that every thread acquire()s from by reference — there is
    // no scenario in this codebase where duplicating or relocating a pool
    // after construction makes sense.
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    ConnectionPool(ConnectionPool&&) = delete;
    ConnectionPool& operator=(ConnectionPool&&) = delete;

    // Blocks (does not error or spin) if every connection is currently
    // checked out, until another thread's lease is destroyed and frees one
    // up. Returns a move-only ConnectionLease that returns its connection
    // automatically when it goes out of scope.
    //
    // Precondition the pool does not (yet) enforce: this ConnectionPool
    // must outlive every ConnectionLease it hands out. A lease only stores
    // a raw ConnectionPool* back-pointer (see ConnectionLease::pool_) — if
    // the pool is destroyed while a lease is still alive (e.g. a request
    // still in flight during server shutdown), that lease's destructor
    // later dereferences a dangling pointer. S3 (wiring the pool into
    // main()/handlers) must ensure the pool outlives every in-flight
    // request before this matters in practice; flagging it here now since
    // nothing in this type's API currently prevents it.
    ConnectionLease acquire();

private:
    // Only a ConnectionLease's destructor/move-assignment calls this, to
    // hand a slot back and wake one waiter. Not part of the public API —
    // callers never do this bookkeeping themselves.
    friend class ConnectionLease;
    void release(std::size_t index);

    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<pqxx::connection> connections_;
    std::vector<bool> in_use_;
};
