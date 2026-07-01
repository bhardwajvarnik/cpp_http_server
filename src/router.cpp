#include "router.hpp"
#include <fstream>
#include <sstream>

void Router::addRoute(const std::string& method, const std::string& path, Handler handler) {
    routes_[method + " " + path] = std::move(handler);
}

void Router::serveStatic(const std::string& url_prefix, const std::string& dir_path) {
    static_prefix_ = url_prefix;
    static_dir_ = dir_path;
}

HttpResponse Router::route(const HttpRequest& req) const {
    auto it = routes_.find(req.method + " " + req.path);
    if (it != routes_.end()) {
        return it->second(req);
    }

    if (!static_prefix_.empty() && req.method == "GET" &&
        req.path.rfind(static_prefix_, 0) == 0) {
        return serveStaticFile(req.path);
    }

    return HttpResponse::notFound();
}

HttpResponse Router::serveStaticFile(const std::string& path) const {
    std::string relative = path.substr(static_prefix_.size());
    if (relative.empty() || relative == "/") relative = "/index.html";

    // Basic directory traversal guard.
    if (relative.find("..") != std::string::npos) {
        return HttpResponse::badRequest();
    }

    std::string full_path = static_dir_ + relative;
    std::ifstream file(full_path, std::ios::binary);
    if (!file) {
        return HttpResponse::notFound();
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    std::string content_type = "application/octet-stream";
    auto ends_with = [&](const std::string& suffix) {
        return full_path.size() >= suffix.size() &&
               full_path.compare(full_path.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    if (ends_with(".html")) content_type = "text/html";
    else if (ends_with(".css")) content_type = "text/css";
    else if (ends_with(".js")) content_type = "application/javascript";
    else if (ends_with(".json")) content_type = "application/json";
    else if (ends_with(".txt")) content_type = "text/plain";

    HttpResponse res(200, "OK");
    res.setBody(content, content_type);
    return res;
}
