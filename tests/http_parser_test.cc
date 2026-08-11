#include "http_parser.h"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

void expect(bool condition, std::string_view test_name) {
    if (!condition) {
        std::cerr << "[FAILED] " << test_name << '\n';
        std::exit(EXIT_FAILURE);
    }
    std::cout << "[PASSED] " << test_name << '\n';
}

HttpRequest parse_complete_request(std::string_view input,
                                   std::string_view test_name) {
    HttpRequest request{};
    expect(parse_header_fields(input, request) ==
               HeaderFieldsParseStatus::KComplete,
           test_name);
    return request;
}

void test_complete_line() {
    HttpRequestLine result;
    expect(parse_line("GET /index.html HTTP/1.1\r\n", result) ==
               LineParseStatus::KComplete,
           "complete request line status");
    expect(result.method == "GET", "request line method");
    expect(result.target == "/index.html", "request line target");
    expect(result.version == "HTTP/1.1", "request line version");
}

void test_empty_method() {
    HttpRequestLine result;
    expect(parse_line(" /index.html HTTP/1.1\r\n", result) ==
               LineParseStatus::KBadRequest,
           "empty method rejected");
}

void test_empty_target() {
    HttpRequestLine result;
    expect(parse_line("GET  HTTP/1.1\r\n", result) ==
               LineParseStatus::KBadRequest,
           "empty target rejected");
}

void test_empty_version() {
    HttpRequestLine result;
    expect(parse_line("GET /index.html \r\n", result) ==
               LineParseStatus::KBadRequest,
           "empty version rejected");
}

void test_invalid_version() {
    HttpRequestLine result;
    expect(parse_line("GET /index.html HTTP/2\r\n", result) ==
               LineParseStatus::KBadRequest,
           "invalid HTTP version rejected");
}

void test_request_line_is_saved() {
    const auto request = parse_complete_request(
        "GET /saved HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n",
        "request with saved request line parses");
    expect(request.request_line.method == "GET", "saved request method");
    expect(request.request_line.target == "/saved", "saved request target");
    expect(request.request_line.version == "HTTP/1.1", "saved request version");
}

void test_host_not_first_header() {
    const auto request = parse_complete_request(
        "GET / HTTP/1.1\r\n"
        "User-Agent: test\r\n"
        "Host: localhost\r\n"
        "\r\n",
        "Host after another header parses");
    expect(request.host == "localhost", "Host after another header saved");
}

void test_mixed_case_host() {
    const auto request = parse_complete_request(
        "GET / HTTP/1.1\r\n"
        "hOsT: localhost\r\n"
        "\r\n",
        "mixed-case Host parses");
    expect(request.host == "localhost", "mixed-case Host saved");
}

void test_host_without_space() {
    const auto request = parse_complete_request(
        "GET / HTTP/1.1\r\n"
        "Host:localhost\r\n"
        "\r\n",
        "Host without a space parses");
    expect(request.host == "localhost", "Host without a space saved");
}

void test_header_value_trims_spaces_and_tabs() {
    const auto request = parse_complete_request(
        "GET / HTTP/1.1\r\n"
        "Host:\t localhost \t\r\n"
        "\r\n",
        "header optional whitespace parses");
    expect(request.host == "localhost", "header optional whitespace trimmed");
}

void test_header_missing_colon() {
    HttpRequest request{};
    expect(parse_header_fields(
               "GET / HTTP/1.1\r\n"
               "Host localhost\r\n"
               "\r\n",
               request) == HeaderFieldsParseStatus::KBadRequest,
           "header missing colon rejected");
}

void test_empty_header_name() {
    HttpRequest request{};
    expect(parse_header_fields(
               "GET / HTTP/1.1\r\n"
               ": localhost\r\n"
               "\r\n",
               request) == HeaderFieldsParseStatus::KBadRequest,
           "empty header name rejected");
}

void test_x_host_does_not_replace_host() {
    HttpRequest request{};
    expect(parse_header_fields(
               "GET / HTTP/1.1\r\n"
               "X-Host: localhost\r\n"
               "\r\n",
               request) == HeaderFieldsParseStatus::KBadRequest,
           "X-Host cannot replace Host");
    expect(request.host.empty(), "X-Host not stored as Host");
}

void test_unknown_header_is_ignored() {
    const auto request = parse_complete_request(
        "GET / HTTP/1.1\r\n"
        "X-Unknown: value:with:colons\r\n"
        "Host: localhost\r\n"
        "\r\n",
        "unknown header ignored");
    expect(request.host == "localhost", "Host parsed after unknown header");
}

void test_http_11_defaults_to_keep_alive() {
    const auto request = parse_complete_request(
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n",
        "HTTP/1.1 default connection parses");
    expect(request.keep_alive, "HTTP/1.1 defaults to keep-alive");
}

void test_connection_close_lowercase() {
    const auto request = parse_complete_request(
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n"
        "\r\n",
        "lowercase Connection close parses");
    expect(!request.keep_alive, "Connection: close disables keep-alive");
}

void test_connection_close_mixed_case() {
    const auto request = parse_complete_request(
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: Close\r\n"
        "\r\n",
        "mixed-case Connection close parses");
    expect(!request.keep_alive, "Connection: Close disables keep-alive");
}

void test_http_10_defaults_to_close() {
    const auto request = parse_complete_request(
        "GET / HTTP/1.0\r\n"
        "\r\n",
        "HTTP/1.0 request parses");
    expect(!request.keep_alive, "HTTP/1.0 defaults to close");
}

}  // namespace

int main() {
    test_complete_line();
    test_empty_method();
    test_empty_target();
    test_empty_version();
    test_invalid_version();
    test_request_line_is_saved();
    test_host_not_first_header();
    test_mixed_case_host();
    test_host_without_space();
    test_header_value_trims_spaces_and_tabs();
    test_header_missing_colon();
    test_empty_header_name();
    test_x_host_does_not_replace_host();
    test_unknown_header_is_ignored();
    test_http_11_defaults_to_keep_alive();
    test_connection_close_lowercase();
    test_connection_close_mixed_case();
    test_http_10_defaults_to_close();
    std::cout << "All HTTP parser tests passed.\n";
    return EXIT_SUCCESS;
}
