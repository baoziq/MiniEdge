#include "static_file_handler.h"
#include "proxy_server.h"

#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>

std::string handle_static_request(const HttpRequest &request) {
    std::cout << "handle_static_request\n";
    std::cout << "method: " << request.request_line.method << std::endl;
    if (request.request_line.method != "GET") {
        return "HTTP/1.1 405 OK\r\nContent-Type: text/plain\r\n";
    }
    std::string file_path = request.request_line.target;
    if (file_path.size() <= 8) {
        return "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n";
    }
    file_path = file_path.substr(8);
    auto arg_it = file_path.find('?');
    if (arg_it != std::string::npos) {
        file_path = file_path.erase(arg_it);
    }
    file_path = basic_path + file_path;
    std::cout << "file_path: " << file_path << std::endl;
    int file_fd = open(file_path.c_str(), O_RDONLY);
    if (file_fd == -1) {
        return "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n";
    }
    struct stat st;
    if (fstat(file_fd, &st) == -1) {
        // error
        return "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n";
    }
    std::string response_header = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(st.st_size ) + "\r\nConnection: close\r\n\r\n";
    std::string response_body;
    char buf[4096];
    ssize_t n;
    while ((n = read(file_fd, buf, sizeof(buf))) > 0) {
        response_body.append(buf, static_cast<size_t>(n));
    }
    return response_header + response_body;
}