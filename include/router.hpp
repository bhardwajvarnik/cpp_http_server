#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include "http_request.hpp"
#include "http_response.hpp"

using Handler = std::function<HttpResponse(const HttpRequest&)>;

// Maps "METHOD path" -> handler, with an optional static file fallback.
class Router {
public:
    void addRoute(const std::string& method, const std::string& path, Handler handler);
    HttpResponse route(const HttpRequest& req) const;

    // Serves files under dir_path for any GET request whose path starts
    // with url_prefix, e.g. serveStatic("/static", "./public").
    void serveStatic(const std::string& url_prefix, const std::string& dir_path);

private:
    std::unordered_map<std::string, Handler> routes_;
    std::string static_prefix_;
    std::string static_dir_;

    HttpResponse serveStaticFile(const std::string& path) const;
};
