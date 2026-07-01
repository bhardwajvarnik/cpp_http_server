#include "http_request.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace {

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                    [](unsigned char c) { return std::tolower(c); });
    return s;
}

}  // namespace

bool HttpRequest::parse(const std::string& raw, HttpRequest& out) {
    size_t header_end = raw.find("\r\n\r\n");
    std::string head = (header_end == std::string::npos) ? raw : raw.substr(0, header_end);

    std::istringstream stream(head);
    std::string line;

    if (!std::getline(stream, line)) return false;
    if (!line.empty() && line.back() == '\r') line.pop_back();

    std::istringstream request_line(line);
    if (!(request_line >> out.method >> out.path >> out.version)) return false;
    if (out.method.empty() || out.path.empty()) return false;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string key = toLower(trim(line.substr(0, colon)));
        std::string value = trim(line.substr(colon + 1));
        out.headers[key] = value;
    }

    if (header_end != std::string::npos) {
        out.body = raw.substr(header_end + 4);
    }

    return true;
}

bool HttpRequest::keepAlive() const {
    auto it = headers.find("connection");
    if (it == headers.end()) {
        return version == "HTTP/1.1";
    }
    return toLower(it->second).find("keep-alive") != std::string::npos;
}
