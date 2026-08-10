#include "http_parser.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

void expect(bool condition, std::string_view test_name) {
    if (!condition) {
        std::cerr << "[FAILED]" << test_name << "\n";
        std::exit(EXIT_FAILURE);
    }
    std::cout << "[PASSED]" << test_name << "\n";
}

void test_complete_line() {
    const std::string input = "GET /index.html HTTP/1.1\r\n";
    HttpRequestLine res;
    const auto result = parse_line(input, res);
    std::cout << "method" << res.method << "\n";
    expect(res.method == "GET", "method");
    expect(res.target == "/index.html", "target");
    expect(res.version == "HTTP/1.1", "version");
    expect(result == LineParseStatus::KComplete,
        "complete line status");    
}

void test_missing_method() {
    const std::string input = "/index.html HTTP/1.1\r\n";
    HttpRequestLine res;
    const auto result = parse_line(input, res);
    expect(result == LineParseStatus::KBadRequest, "miss method");
}

void test_missing_target() {
    const std::string input = "/index.html HTTP/1.1\r\n";
    HttpRequestLine res;
    const auto result = parse_line(input, res);
    expect(result == LineParseStatus::KBadRequest, "miss target");
}

void test_missing_version() {
    const std::string input = "/index.html HTTP/1.1\r\n";
    HttpRequestLine res;
    const auto result = parse_line(input, res);
    expect(result == LineParseStatus::KBadRequest, "miss version");
}

void test_complete_header() {
    const std::string_view input = "GET /index.html HTTP/1.1\r\n"
                                "Host: localhost\r\n"
                                "Connection: close\r\n"
                                "User-Agent: curl/8.0\r\n"
                                "\r\n";
    HttpRequest request;
    const auto res = parse_header_fields(input, request);
    expect(request.host == "localhost", "test_complete_headerhost");
    expect(request.connection == "close", "test_complete_headerconnection");
    expect(res == HeaderFieldsParseStatus::KClose, "test_complete_headerstatus");
    
}

void test_lower_header() {
    const std::string_view input = "GET /index.html HTTP/1.1\r\n"
                                "host: localhost\r\n"
                                "connection: close\r\n"
                                "User-Agent: curl/8.0\r\n"
                                "\r\n";
    HttpRequest request;
    const auto res = parse_header_fields(input, request);
    expect(res == HeaderFieldsParseStatus::KClose, "test_lower_headerstatus");
    expect(request.host == "localhost", "test_lower_headerhost");
    expect(request.connection == "close", "test_lower_headerconnection");
}

void test_bad_request_header() {
    const std::string_view input = "GET /index.html HTTP/1.1\r\n"
                                "connection: close\r\n"
                                "User-Agent: curl/8.0\r\n"
                                "\r\n";
    HttpRequest request;
    const auto res = parse_header_fields(input, request);
    expect(res == HeaderFieldsParseStatus::KBadRequest, "status");
    expect(request.host == "", "host");
}

void test_nonclose_header() {
    const std::string_view input = "GET /index.html HTTP/1.1\r\n"
                                "Host: localhost\r\n"
                                "User-Agent: curl/8.0\r\n"
                                "\r\n";
    HttpRequest request;
    const auto res = parse_header_fields(input, request);
    expect(res == HeaderFieldsParseStatus::KComplete, "test_bad_request_headerstatus");
}

int main() {
    test_complete_line();
    test_missing_target();
    test_missing_version();
    test_complete_header();
    test_lower_header();
    test_bad_request_header();
    test_nonclose_header();
    std::cout << "All HTTP parser tests passed.\n";
}