#pragma once
#include <string>
#include <unordered_map>

// Builds a well-formed HTTP/1.1 response.
class HttpResponse {
public:
    explicit HttpResponse(int status_code = 200, const std::string& status_text = "OK");

    void setHeader(const std::string& key, const std::string& value);
    void setBody(const std::string& body, const std::string& content_type = "text/plain");
    void setKeepAlive(bool keep_alive);

    std::string toString() const;

    static HttpResponse notFound();
    static HttpResponse badRequest();
    static HttpResponse serverError();

private:
    int status_code_;
    std::string status_text_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
};
