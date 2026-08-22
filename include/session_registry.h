#pragma once
#include "unique_fd.h"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

enum class ProxyState{
    // 正在读取客户端请求
    KReadingRequest,
    // 正在连接上游
    KConnectingUpstream,
    // 正在向上游发送请求
    KSendingRequest,
    // 正在读取上游响应
    KReadingResponse,
    // 正在给客户端发送响应
    KSendingResponse,
    KClosing
};

struct ProxySession{
    int client_fd = -1;
    std::optional<UniqueFd> upstream_fd;
    ProxyState state{ProxyState::KReadingRequest};
    std::string upstream_output;
    std::size_t upstream_write_offset{0};
    bool upstream_response_started{false};
};

class SessionRegistry {
public:
    // 把client_fd加入client到上游的映射表中
    bool create(int client_fd);
    bool bind_upstream(int client_fd, UniqueFd upstream_fd);
    bool unbind_upstream(int client_fd);

    ProxySession* find_by_client(int client_fd);
    ProxySession* find_by_upstream(int upstream_fd);

    bool erase_by_client(int client_fd);
private:
    std::unordered_map<int, ProxySession> sessions_;
    std::unordered_map<int, int> upstream_to_client_;
};
