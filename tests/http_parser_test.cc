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

int main() {
    test_complete_line();
    test_missing_target();
    test_missing_version();
    std::cout << "All HTTP parser tests passed.\n";
}