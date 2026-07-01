#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

// A fixed-size pool of worker threads pulling tasks off a shared queue.
// Used to hand off each accepted connection to a worker instead of
// blocking the single accept loop (Stage 4 of the build).
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads);
    ~ThreadPool();

    void enqueue(std::function<void()> task);

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_{false};

    void workerLoop();
};
