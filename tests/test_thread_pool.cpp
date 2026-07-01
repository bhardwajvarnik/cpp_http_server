#include "thread_pool.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>
#include <iostream>
#include <condition_variable>
#include <mutex>

int main() {
    ThreadPool pool(4);
    std::atomic<int> counter{0};
    const int N = 2000;

    std::mutex m;
    std::condition_variable cv;
    std::atomic<int> completed{0};

    for (int i = 0; i < N; ++i) {
        pool.enqueue([&] {
            counter.fetch_add(1);
            if (completed.fetch_add(1) + 1 == N) cv.notify_one();
        });
    }

    {
        std::unique_lock<std::mutex> lock(m);
        cv.wait_for(lock, std::chrono::seconds(5), [&] { return completed.load() == N; });
    }

    assert(counter.load() == N);
    std::cout << "[PASS] test_thread_pool: " << counter.load() << "/" << N
              << " tasks completed\n";
    return 0;
}
