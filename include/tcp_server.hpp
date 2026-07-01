#pragma once
#include <cstdint>
#include <string>
#include <memory>
#include "thread_pool.hpp"
#include "router.hpp"
#include "load_balancer.hpp"

// Accepts TCP connections on a listening socket and hands each one off to a
// thread pool worker. Supports keep-alive (multiple requests per connection)
// and can operate either as a normal HTTP server (Router) or as a reverse
// proxy / load balancer (LoadBalancer), never both at once.
class TcpServer {
public:
    TcpServer(const std::string& host, uint16_t port, size_t num_threads);
    ~TcpServer();

    bool start();
    void run();
    void stop();

    Router& router() { return router_; }
    void enableLoadBalancer(std::shared_ptr<LoadBalancer> lb);

private:
    std::string host_;
    uint16_t port_;
    int listen_fd_ = -1;
    ThreadPool pool_;
    Router router_;
    std::shared_ptr<LoadBalancer> lb_;
    bool running_ = false;

    void handleConnection(int client_fd);

    static constexpr int kMaxKeepAliveRequests = 100;
    static constexpr int kReadTimeoutSec = 30;
    static constexpr size_t kMaxHeaderBytes = 1024 * 1024;  // 1 MB guard
};
