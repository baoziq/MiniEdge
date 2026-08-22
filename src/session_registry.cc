#include "session_registry.h"

#include <utility>


bool SessionRegistry::create(int client_fd) {
    if (client_fd < 0) {
        return false;
    }

    ProxySession session{};
    session.client_fd = client_fd;
    const auto [it, inserted] =
        sessions_.try_emplace(client_fd, std::move(session));
    (void)it;
    return inserted;
}

bool SessionRegistry::bind_upstream(int client_fd, UniqueFd upstream_fd) {
    auto session_it = sessions_.find(client_fd);
    if (session_it == sessions_.end() ||
        session_it->second.upstream_fd.has_value() ||
        !upstream_fd.valid()) {
        return false;
    }

    const int upstream_fd_number = upstream_fd.get();
    if (upstream_fd_number == client_fd ||
        upstream_to_client_.find(upstream_fd_number) !=
            upstream_to_client_.end()) {
        return false;
    }

    const auto [mapping_it, inserted] =
        upstream_to_client_.try_emplace(upstream_fd_number, client_fd);
    (void)mapping_it;
    if (!inserted) {
        return false;
    }

    session_it->second.upstream_fd.emplace(std::move(upstream_fd));
    return true;
}

bool SessionRegistry::unbind_upstream(int client_fd) {
    auto session_it = sessions_.find(client_fd);
    if (session_it == sessions_.end() ||
        !session_it->second.upstream_fd.has_value()) {
        return false;
    }

    const int upstream_fd = session_it->second.upstream_fd->get();
    upstream_to_client_.erase(upstream_fd);
    session_it->second.upstream_fd.reset();
    session_it->second.upstream_output.clear();
    session_it->second.upstream_write_offset = 0;
    return true;
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

    if (it->second.upstream_fd.has_value()) {
        const int upstream_fd = it->second.upstream_fd->get();
        upstream_to_client_.erase(upstream_fd);
    }

    sessions_.erase(it);
    return true;
}
