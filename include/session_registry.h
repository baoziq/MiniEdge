#pragma once
#include "unique_fd.h"

#include <optional>
#include <unordered_map>
#include <string>

enum class ProxyState{
    KReadingRequest,
    KConnectingUpstream,
    KSendingRequest,
    KReadingResponse,
    KSendingResponse,
    KClosing
};

struct ProxySession{
    int client_fd = -1;
    std::optional<UniqueFd> upstream_fd;
    ProxyState state{ProxyState::KReadingRequest};
    std::string upstream_output;
    std::size_t upstream_write_offset{0};
};

class SessionRegistry {
public:
    bool create(int client_fd);
    bool bind_upstream(int client_fd, UniqueFd upstream_fd);

    ProxySession* find_by_client(int client_fd);
    ProxySession* find_by_upstream(int upstream_fd);

    bool erase_by_client(int client_fd);
private:
    std::unordered_map<int, ProxySession> sessions_;
    std::unordered_map<int, int> upstream_to_client_;
};