#include "tcp_server.hpp"
#include "http_request.hpp"
#include "http_response.hpp"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <netinet/tcp.h>

TcpServer::TcpServer(const std::string& host, uint16_t port, size_t num_threads)
    : host_(host), port_(port), pool_(num_threads) {}

TcpServer::~TcpServer() { stop(); }

void TcpServer::enableLoadBalancer(std::shared_ptr<LoadBalancer> lb) {
    lb_ = std::move(lb);
}

bool TcpServer::start() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        std::cerr << "socket() failed: " << strerror(errno) << "\n";
        return false;
    }

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = (host_ == "0.0.0.0") ? INADDR_ANY : inet_addr(host_.c_str());

    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "bind() failed: " << strerror(errno) << "\n";
        return false;
    }

    if (listen(listen_fd_, 512) < 0) {
        std::cerr << "listen() failed: " << strerror(errno) << "\n";
        return false;
    }

    running_ = true;
    std::cout << "[server] listening on " << host_ << ":" << port_ << "\n";
    return true;
}

void TcpServer::stop() {
    running_ = false;
    if (listen_fd_ != -1) {
        close(listen_fd_);
        listen_fd_ = -1;
    }
}

void TcpServer::run() {
    while (running_) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_fd < 0) {
            if (running_) std::cerr << "accept() failed: " << strerror(errno) << "\n";
            continue;
        }

        // Disable Nagle's algorithm: HTTP responses are typically small and
        // latency-sensitive, so we don't want the kernel batching writes.
        int flag = 1;
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

        pool_.enqueue([this, client_fd] { handleConnection(client_fd); });
    }
}

void TcpServer::handleConnection(int client_fd) {
    timeval tv{kReadTimeoutSec, 0};
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    std::string buffer;
    char chunk[8192];
    int requests_served = 0;

    while (requests_served < kMaxKeepAliveRequests) {
        size_t header_end;
        while ((header_end = buffer.find("\r\n\r\n")) == std::string::npos) {
            ssize_t n = read(client_fd, chunk, sizeof(chunk));
            if (n <= 0) { close(client_fd); return; }
            buffer.append(chunk, static_cast<size_t>(n));
            if (buffer.size() > kMaxHeaderBytes) {
                std::string res = HttpResponse::badRequest().toString();
                ssize_t written = write(client_fd, res.c_str(), res.size());
                (void)written;
                close(client_fd);
                return;
            }
        }

        HttpRequest req;
        if (!HttpRequest::parse(buffer, req)) {
            std::string res = HttpResponse::badRequest().toString();
            ssize_t written = write(client_fd, res.c_str(), res.size());
            (void)written;  // best-effort; connection is closing regardless
            close(client_fd);
            return;
        }

        size_t content_length = 0;
        auto it = req.headers.find("content-length");
        if (it != req.headers.end()) {
            try { content_length = std::stoul(it->second); } catch (...) { content_length = 0; }
        }

        size_t body_have = buffer.size() - (header_end + 4);
        while (body_have < content_length) {
            ssize_t n = read(client_fd, chunk, sizeof(chunk));
            if (n <= 0) { close(client_fd); return; }
            buffer.append(chunk, static_cast<size_t>(n));
            body_have += static_cast<size_t>(n);
        }
        req.body = buffer.substr(header_end + 4, content_length);

        bool keep_alive = req.keepAlive();
        std::string res_str;

        if (lb_) {
            res_str = lb_->forward(req);
        } else {
            HttpResponse res = router_.route(req);
            res.setKeepAlive(keep_alive);
            res_str = res.toString();
        }

        if (write(client_fd, res_str.c_str(), res_str.size()) < 0) {
            close(client_fd);
            return;
        }

        requests_served++;
        if (!keep_alive) { close(client_fd); return; }

        // Drop the consumed request; keep any pipelined bytes that follow.
        buffer.erase(0, header_end + 4 + content_length);
    }

    close(client_fd);
}
