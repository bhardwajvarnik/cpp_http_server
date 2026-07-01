#include "load_balancer.hpp"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <thread>
#include <chrono>

LoadBalancer::LoadBalancer(LbStrategy strategy) : strategy_(strategy) {}

void LoadBalancer::addBackend(const std::string& host, uint16_t port) {
    auto b = std::make_unique<Backend>();
    b->host = host;
    b->port = port;
    backends_.push_back(std::move(b));
}

Backend* LoadBalancer::pickBackend() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (backends_.empty()) return nullptr;

    if (strategy_ == LbStrategy::RoundRobin) {
        for (size_t tries = 0; tries < backends_.size(); ++tries) {
            size_t idx = rr_index_.fetch_add(1) % backends_.size();
            if (backends_[idx]->healthy.load()) return backends_[idx].get();
        }
        return nullptr;
    }

    // Least connections
    Backend* best = nullptr;
    for (auto& b : backends_) {
        if (!b->healthy.load()) continue;
        if (!best || b->active_connections.load() < best->active_connections.load()) {
            best = b.get();
        }
    }
    return best;
}

std::string LoadBalancer::forward(const HttpRequest& req) {
    Backend* backend = pickBackend();
    if (!backend) {
        HttpResponse res(503, "Service Unavailable");
        res.setBody("503 Service Unavailable: no healthy backends", "text/plain");
        return res.toString();
    }

    backend->active_connections.fetch_add(1);
    HttpResponse res = sendToBackend(backend, req);
    backend->active_connections.fetch_sub(1);
    return res.toString();
}

HttpResponse LoadBalancer::sendToBackend(Backend* backend, const HttpRequest& req) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return HttpResponse::serverError();

    timeval tv{5, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(backend->port);
    inet_pton(AF_INET, backend->host.c_str(), &addr.sin_addr);

    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        backend->healthy.store(false);
        close(sock);
        HttpResponse res(502, "Bad Gateway");
        res.setBody("502 Bad Gateway: backend unreachable", "text/plain");
        return res;
    }

    std::ostringstream out;
    out << req.method << " " << req.path << " HTTP/1.1\r\n";
    out << "Host: " << backend->host << ":" << backend->port << "\r\n";
    for (const auto& [k, v] : req.headers) {
        if (k == "connection") continue;  // we manage this ourselves
        out << k << ": " << v << "\r\n";
    }
    out << "Connection: close\r\n\r\n" << req.body;

    std::string raw = out.str();
    if (write(sock, raw.c_str(), raw.size()) < 0) {
        close(sock);
        return HttpResponse::serverError();
    }

    std::string response;
    char chunk[8192];
    ssize_t n;
    while ((n = read(sock, chunk, sizeof(chunk))) > 0) {
        response.append(chunk, static_cast<size_t>(n));
    }
    close(sock);

    size_t header_end = response.find("\r\n\r\n");
    HttpResponse res(200, "OK");
    res.setBody(header_end == std::string::npos ? response
                                                 : response.substr(header_end + 4));
    return res;
}

void LoadBalancer::healthCheckLoop() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        for (auto& b : backends_) {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            timeval tv{1, 0};
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(b->port);
            inet_pton(AF_INET, b->host.c_str(), &addr.sin_addr);
            bool ok = connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
            b->healthy.store(ok);
            close(sock);
        }
    }
}
