#include "http_request.hpp"
#include "http_response.hpp"
#include <cassert>
#include <iostream>

void test_parse_get_request() {
    std::string raw = "GET /hello HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n";
    HttpRequest req;
    bool ok = HttpRequest::parse(raw, req);
    assert(ok);
    assert(req.method == "GET");
    assert(req.path == "/hello");
    assert(req.version == "HTTP/1.1");
    assert(req.headers.at("host") == "localhost");
    assert(req.keepAlive() == true);
    std::cout << "[PASS] test_parse_get_request\n";
}

void test_parse_post_with_body() {
    std::string raw = "POST /submit HTTP/1.1\r\nContent-Length: 5\r\n\r\nhello";
    HttpRequest req;
    bool ok = HttpRequest::parse(raw, req);
    assert(ok);
    assert(req.method == "POST");
    assert(req.body == "hello");
    std::cout << "[PASS] test_parse_post_with_body\n";
}

void test_connection_close_respected() {
    std::string raw = "GET / HTTP/1.1\r\nConnection: close\r\n\r\n";
    HttpRequest req;
    HttpRequest::parse(raw, req);
    assert(req.keepAlive() == false);
    std::cout << "[PASS] test_connection_close_respected\n";
}

void test_http10_defaults_to_close() {
    std::string raw = "GET / HTTP/1.0\r\n\r\n";
    HttpRequest req;
    HttpRequest::parse(raw, req);
    assert(req.keepAlive() == false);
    std::cout << "[PASS] test_http10_defaults_to_close\n";
}

void test_malformed_request_line_rejected() {
    std::string raw = "GARBAGE\r\n\r\n";
    HttpRequest req;
    bool ok = HttpRequest::parse(raw, req);
    assert(!ok);
    std::cout << "[PASS] test_malformed_request_line_rejected\n";
}

void test_response_serialization() {
    HttpResponse res(200, "OK");
    res.setBody("hi", "text/plain");
    std::string out = res.toString();
    assert(out.find("HTTP/1.1 200 OK") != std::string::npos);
    assert(out.find("Content-Length: 2") != std::string::npos);
    assert(out.find("hi") != std::string::npos);
    std::cout << "[PASS] test_response_serialization\n";
}

void test_not_found() {
    HttpResponse res = HttpResponse::notFound();
    assert(res.toString().find("404") != std::string::npos);
    std::cout << "[PASS] test_not_found\n";
}

int main() {
    test_parse_get_request();
    test_parse_post_with_body();
    test_connection_close_respected();
    test_http10_defaults_to_close();
    test_malformed_request_line_rejected();
    test_response_serialization();
    test_not_found();
    std::cout << "\nAll HTTP parser tests passed!\n";
    return 0;
}
