#include "session_registry.h"
#include "unique_fd.h"
#include <optional>

bool SessionRegistry::create(int client_fd) {
    ProxySession session{client_fd, std::nullopt};
    auto [it, inserted] = sessions_.try_emplace(client_fd, std::move(session));
    return inserted;
}

bool SessionRegistry::bind_upstream(int client_fd, UniqueFd upstream_fd) {
    auto it = sessions_.find(client_fd);
    if (it == sessions_.end() || it->second.upstream_fd.has_value()) {
        return false;
    }
    
    it->second.upstream_fd = std::move(upstream_fd);
    auto [it1, inserted] = upstream_to_client_.try_emplace(it->second.upstream_fd->get(), client_fd);
    return inserted;
}

ProxySession* SessionRegistry::find_by_client(int client_fd) {
    auto it = sessions_.find(client_fd);
    if (it == sessions_.end()) {
        return nullptr;
    }
    return &it->second;
}

ProxySession* SessionRegistry::find_by_upstream(int upstream_fd) {
    auto it = upstream_to_client_.find(upstream_fd);
    if (it == upstream_to_client_.end()) {
        return nullptr;
    }
    int client_fd = it->second;
    auto it1 = sessions_.find(client_fd);
    if (it1 == sessions_.end()) {
        return nullptr;
    }
    return &it1->second;
}

bool SessionRegistry::erase_by_client(int client_fd) {
    auto it = sessions_.find(client_fd);
    if (it == sessions_.end()) {
        return false;
    }
    if (!it->second.upstream_fd.has_value()) {
        return false;
    }
    if (it->second.upstream_fd.has_value()) {
        int upstream_fd = it->second.upstream_fd->get();
        upstream_to_client_.erase(upstream_fd);
    }
    sessions_.erase(it);
    return true;
}