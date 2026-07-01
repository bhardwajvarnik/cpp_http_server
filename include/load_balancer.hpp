#pragma once
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <cstdint>
#include "http_request.hpp"
#include "http_response.hpp"

struct Backend {
    std::string host;
    uint16_t port;
    std::atomic<int> active_connections{0};
    std::atomic<bool> healthy{true};
};

enum class LbStrategy { RoundRobin, LeastConnections };

// Reverse proxy that forwards requests to a pool of backend servers.
// NOTE (known simplification): the backend's original response headers
// (aside from the body) are not passed through verbatim -- we re-wrap the
// body in our own HttpResponse. Good enough for a portfolio project; a
// production proxy would forward headers 1:1.
class LoadBalancer {
public:
    explicit LoadBalancer(LbStrategy strategy = LbStrategy::RoundRobin);

    void addBackend(const std::string& host, uint16_t port);
    std::string forward(const HttpRequest& req);

    // Run on a background thread; periodically pings each backend and
    // marks it healthy/unhealthy so pickBackend() can skip dead nodes.
    void healthCheckLoop();

private:
    std::vector<std::unique_ptr<Backend>> backends_;
    std::mutex mutex_;
    std::atomic<size_t> rr_index_{0};
    LbStrategy strategy_;

    Backend* pickBackend();
    HttpResponse sendToBackend(Backend* backend, const HttpRequest& req);
};
