#include "tcp_server.hpp"
#include "router.hpp"
#include "load_balancer.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <memory>

namespace {

void printUsage(const char* prog) {
    std::cout
        << "Usage:\n"
        << "  " << prog << " --port 8080 --threads 4 [--static ./public]\n"
        << "  " << prog << " --port 8080 --lb round-robin"
                            " --backend 127.0.0.1:9001 --backend 127.0.0.1:9002\n"
        << "\nOptions:\n"
        << "  --port <n>          Listen port (default 8080)\n"
        << "  --threads <n>       Worker thread pool size (default: hardware concurrency)\n"
        << "  --static <dir>      Serve static files under /static from this directory\n"
        << "  --lb [strategy]     Enable reverse-proxy mode. strategy = round-robin"
                                    " (default) | least-connections\n"
        << "  --backend host:port Add a backend (repeatable, only used with --lb)\n";
}

}  // namespace

int main(int argc, char** argv) {
    uint16_t port = 8080;
    size_t threads = std::thread::hardware_concurrency();
    if (threads == 0) threads = 4;
    std::string static_dir;
    bool lb_mode = false;
    LbStrategy strategy = LbStrategy::RoundRobin;
    std::vector<std::pair<std::string, uint16_t>> backends;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--threads" && i + 1 < argc) {
            threads = std::stoul(argv[++i]);
        } else if (arg == "--static" && i + 1 < argc) {
            static_dir = argv[++i];
        } else if (arg == "--lb") {
            lb_mode = true;
            if (i + 1 < argc && std::string(argv[i + 1]) == "least-connections") {
                strategy = LbStrategy::LeastConnections;
                ++i;
            } else if (i + 1 < argc && std::string(argv[i + 1]) == "round-robin") {
                ++i;
            }
        } else if (arg == "--backend" && i + 1 < argc) {
            std::string b = argv[++i];
            size_t colon = b.find(':');
            if (colon == std::string::npos) {
                std::cerr << "Invalid --backend value, expected host:port\n";
                return 1;
            }
            backends.emplace_back(b.substr(0, colon),
                                   static_cast<uint16_t>(std::stoi(b.substr(colon + 1))));
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
    }

    TcpServer server("0.0.0.0", port, threads);
    if (!server.start()) return 1;

    if (lb_mode) {
        auto lb = std::make_shared<LoadBalancer>(strategy);
        for (auto& [host, bport] : backends) lb->addBackend(host, bport);
        std::thread(&LoadBalancer::healthCheckLoop, lb).detach();
        server.enableLoadBalancer(lb);
        std::cout << "[mode] reverse proxy / load balancer, " << backends.size()
                  << " backend(s), strategy="
                  << (strategy == LbStrategy::RoundRobin ? "round-robin" : "least-connections")
                  << "\n";
    } else {
        Router& router = server.router();

        router.addRoute("GET", "/", [](const HttpRequest&) {
            HttpResponse res(200, "OK");
            res.setBody("Welcome to the C++ HTTP server!\n", "text/plain");
            return res;
        });

        router.addRoute("GET", "/health", [](const HttpRequest&) {
            HttpResponse res(200, "OK");
            res.setBody(R"({"status":"ok"})", "application/json");
            return res;
        });

        router.addRoute("POST", "/echo", [](const HttpRequest& req) {
            HttpResponse res(200, "OK");
            res.setBody(req.body.empty() ? "no body" : req.body, "text/plain");
            return res;
        });

        if (!static_dir.empty()) {
            router.serveStatic("/static", static_dir);
            std::cout << "[static] serving " << static_dir << " under /static\n";
        }

        std::cout << "[mode] HTTP server, " << threads << " worker threads\n";
    }

    server.run();
    return 0;
}
