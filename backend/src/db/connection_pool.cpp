#include "connection_pool.hpp"

#include <stdexcept>

#include "connection.hpp"

// ── ConnectionLease ──────────────────────────────────────────────────────

ConnectionLease::ConnectionLease(ConnectionPool* pool, std::size_t index)
    : pool_(pool), index_(index) {}

ConnectionLease::ConnectionLease(ConnectionLease&& other) noexcept
    : pool_(other.pool_), index_(other.index_) {
    // Null out the source so its destructor sees "already given back" and
    // does nothing — otherwise both this lease and `other` would believe
    // they own the same slot, and whichever destructor runs second would
    // release a connection that's already back in the pool (or, worse,
    // already checked out again by some third thread).
    other.pool_ = nullptr;
}

ConnectionLease& ConnectionLease::operator=(ConnectionLease&& other) noexcept {
    if (this != &other) {
        // This lease may already hold a different connection — release it
        // before taking over `other`'s, or that slot would leak (marked
        // in_use_ forever, since nothing would ever call release() for it
        // again).
        if (pool_ != nullptr) {
            pool_->release(index_);
        }
        pool_ = other.pool_;
        index_ = other.index_;
        other.pool_ = nullptr;
    }
    return *this;
}

ConnectionLease::~ConnectionLease() {
    if (pool_ != nullptr) {
        pool_->release(index_);
    }
}

pqxx::connection& ConnectionLease::operator*() const { return pool_->connections_[index_]; }

pqxx::connection* ConnectionLease::operator->() const { return &pool_->connections_[index_]; }

// ── ConnectionPool ───────────────────────────────────────────────────────

ConnectionPool::ConnectionPool(std::size_t size) {
    if (size == 0) {
        throw std::invalid_argument("ConnectionPool: size must be at least 1");
    }

    connections_.reserve(size);
    in_use_.resize(size, false);
    for (std::size_t i = 0; i < size; ++i) {
        connections_.push_back(make_connection());
    }
}

ConnectionLease ConnectionPool::acquire() {
    // std::unique_lock, not std::lock_guard: a condition_variable's wait()
    // needs to unlock the mutex while the calling thread is actually
    // blocked (so other threads can get in to flip in_use_ and notify us)
    // and then re-lock it before returning control to us, so we can safely
    // read/write in_use_ again afterward. lock_guard only supports "locked
    // for its whole scope" — it has no unlock()/lock() to hand to wait(),
    // so it's the wrong tool here even though it's the simpler one.
    std::unique_lock<std::mutex> lock(mutex_);

    std::size_t index = 0;

    // cv.wait(lock, predicate) is shorthand for:
    //     while (!predicate()) { cv.wait(lock); }
    // "predicate" here is "is some slot free, and if so, which one" — found
    // by a short linear scan over in_use_. Two things this buys us over a
    // naive spin-poll loop like `while (!found) { check again; }`:
    //   1. No spinning: while predicate() is false, wait() actually blocks
    //      the OS thread (unlocking the mutex first) instead of burning CPU
    //      re-checking in a tight loop.
    //   2. Spurious-wakeup safety: condition variables are allowed by the
    //      standard to wake a waiting thread even when nobody called
    //      notify_*() (a quirk of how they're implemented on top of OS
    //      primitives). The predicate-loop form re-checks the actual
    //      condition every time wait() returns for *any* reason, so a
    //      spurious wakeup just re-locks, finds no slot free, and goes back
    //      to sleep — it can never incorrectly hand out a connection.
    cv_.wait(lock, [this, &index]() {
        for (std::size_t i = 0; i < in_use_.size(); ++i) {
            if (!in_use_[i]) {
                index = i;
                return true;
            }
        }
        return false;
    });

    in_use_[index] = true;
    return ConnectionLease(this, index);
}

void ConnectionPool::release(std::size_t index) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        in_use_[index] = false;
    }
    // Notified outside the lock: whichever thread wait() wakes still has to
    // re-acquire the mutex itself before it can check the predicate, so
    // holding it a moment longer here wouldn't change correctness — only
    // delay the woken thread. notify_one() (not notify_all()) is enough:
    // exactly one slot became free, so waking more than one waiter would
    // just mean the extra thread(s) re-check the predicate, find nothing,
    // and go back to sleep — harmless, but pointless work.
    cv_.notify_one();
}
