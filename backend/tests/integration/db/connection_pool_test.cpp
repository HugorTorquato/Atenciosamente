#include <catch2/catch_test_macros.hpp>

#include "db/connection_pool.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

// Integration tier because ConnectionPool's constructor needs a live
// Postgres, not because this runs any SQL. Each TEST_CASE owns its own pool
// and joins every thread before returning, so nothing leaks between cases.

TEST_CASE("acquire() never checks out more connections than the pool's size under concurrent load",
          "[connection_pool][integration]")
{
    constexpr std::size_t pool_size = 2;
    constexpr int num_threads = 4;
    constexpr int iterations_per_thread = 30;

    ConnectionPool pool(pool_size);

    std::atomic<int> in_use_count{0};
    std::atomic<int> max_observed{0};

    const auto worker = [&]() {
        for (int i = 0; i < iterations_per_thread; ++i) {
            ConnectionLease lease = pool.acquire();

            // CAS loop, not read-then-write: avoids losing an update if two
            // threads both bump max_observed at once.
            const int current = in_use_count.fetch_add(1, std::memory_order_acq_rel) + 1;
            int prev_max = max_observed.load(std::memory_order_relaxed);
            while (current > prev_max &&
                   !max_observed.compare_exchange_weak(prev_max, current, std::memory_order_acq_rel)) {
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(2));

            in_use_count.fetch_sub(1, std::memory_order_acq_rel);
            // `lease` releases its connection back to the pool when it goes
            // out of scope at the end of this iteration.
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker);
    }
    for (std::thread& t : threads) {
        t.join();
    }

    // 4 threads over a 2-slot pool makes overlap near-certain, so this has
    // real power to catch an over-commit bug without racing the assertion
    // itself against scheduling.
    REQUIRE(max_observed.load() >= 1);
    REQUIRE(max_observed.load() <= static_cast<int>(pool_size));
}

TEST_CASE("acquire() blocks until a connection is released when the pool is exhausted",
          "[connection_pool][integration]")
{
    ConnectionPool pool(1);

    std::atomic<bool> about_to_acquire{false};
    std::atomic<bool> acquired{false};
    std::chrono::steady_clock::time_point acquire_time;
    std::chrono::steady_clock::time_point release_time;

    std::thread blocked;
    {
        ConnectionLease held = pool.acquire(); // the pool's only connection

        blocked = std::thread([&]() {
            about_to_acquire.store(true, std::memory_order_release);
            ConnectionLease lease = pool.acquire(); // must block until `held` is released below
            acquire_time = std::chrono::steady_clock::now();
            acquired.store(true, std::memory_order_release);
        });

        while (!about_to_acquire.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        // `held` is still alive, so `acquired` can't legitimately flip yet;
        // this just gives `blocked` time to reach cv.wait().
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        REQUIRE_FALSE(acquired.load(std::memory_order_acquire));

        // Taken before `held` is destroyed (same thread, no race) so it's a
        // safe lower bound — release() notifies outside its lock, so timing
        // this after destruction could race with `blocked` finishing first.
        release_time = std::chrono::steady_clock::now();
    } // `held`'s destructor runs here: returns the connection, wakes `blocked`.

    blocked.join();

    REQUIRE(acquired.load());
    // Confirms blocking actually happened, not just eventual completion.
    REQUIRE(acquire_time >= release_time);
}
