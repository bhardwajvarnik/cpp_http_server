#include "http_response.hpp"
#include <sstream>

HttpResponse::HttpResponse(int status_code, const std::string& status_text)
    : status_code_(status_code), status_text_(status_text) {
    headers_["Server"] = "cpp-http-server/1.0";
}

void HttpResponse::setHeader(const std::string& key, const std::string& value) {
    headers_[key] = value;
}

void HttpResponse::setBody(const std::string& body, const std::string& content_type) {
    body_ = body;
    headers_["Content-Type"] = content_type;
    headers_["Content-Length"] = std::to_string(body.size());
}

void HttpResponse::setKeepAlive(bool keep_alive) {
    headers_["Connection"] = keep_alive ? "keep-alive" : "close";
}

std::string HttpResponse::toString() const {
    std::ostringstream out;
    out << "HTTP/1.1 " << status_code_ << " " << status_text_ << "\r\n";
    for (const auto& [key, value] : headers_) {
        out << key << ": " << value << "\r\n";
    }
    out << "\r\n" << body_;
    return out.str();
}

HttpResponse HttpResponse::notFound() {
    HttpResponse res(404, "Not Found");
    res.setBody("404 Not Found", "text/plain");
    return res;
}

HttpResponse HttpResponse::badRequest() {
    HttpResponse res(400, "Bad Request");
    res.setBody("400 Bad Request", "text/plain");
    return res;
}

HttpResponse HttpResponse::serverError() {
    HttpResponse res(500, "Internal Server Error");
    res.setBody("500 Internal Server Error", "text/plain");
    return res;
}
