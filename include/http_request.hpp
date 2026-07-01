#pragma once
#include <string>
#include <unordered_map>

// Parsed representation of an HTTP/1.1 request.
struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers; // lower-cased keys
    std::string body;

    // HTTP/1.1 defaults to persistent connections unless "Connection: close"
    // is present. HTTP/1.0 defaults the other way.
    bool keepAlive() const;

    // Parses raw bytes (headers must be complete; body may be partial/absent
    // and is filled in separately by the caller once Content-Length is known).
    static bool parse(const std::string& raw, HttpRequest& out);
};
