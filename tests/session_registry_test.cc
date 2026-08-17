#include "session_registry.h"
#include "unique_fd.h"

#include <fcntl.h>
#include <string_view>
#include <cerrno>
#include <iostream>

void expect(bool condition, std::string_view test_name) {
    if (!condition) {
        std::cerr << "[FAILED] " << test_name << '\n';
        std::exit(EXIT_FAILURE);
    }
    std::cout << "[PASSED] " << test_name << '\n';
}

void test_create_new_key() {
    SessionRegistry session{};
    expect(session.create(1), "test_create_new_key");
}

void test_create_old_key() {
    SessionRegistry session{};
    session.create(1);
    expect(session.create(1) == false, "test_create_old_key");

}

void test_bind_new_key() {
    SessionRegistry session{};
    session.create(1);
    UniqueFd upstream_fd(2);
    expect(session.bind_upstream(1, std::move(upstream_fd)), "bind_new_key");

}

void test_bind_old_key() {
    SessionRegistry session{};
    session.create(1);
    UniqueFd upstream_fd(2);
    session.bind_upstream(1, 2);
    expect(!session.bind_upstream(1, std::move(upstream_fd)), "bind_old_key");
}

void test_find_existing_upstream_by_client() {
    SessionRegistry session{};
    session.create(1);
    UniqueFd upstream_fd(2);
    session.bind_upstream(1, 2);
    expect(session.find_by_client(1), "test_find_existing_upstream");
    expect(session.find_by_client(1)->upstream_fd->get() == 2, "find_existing_upstream");
}

void test_find_missing_client_by_client() {
    SessionRegistry session{};
    session.create(1);
    UniqueFd upstream_fd(2);
    session.bind_upstream(1, 2);
    expect(session.find_by_client(2) == nullptr, "test_find_missing_client");
}

void test_find_missing_upstream_by_client() {
    SessionRegistry session{};
    session.create(1);
    UniqueFd upstream_fd(2);
    session.bind_upstream(1, 2);
    expect(session.find_by_upstream(1) == nullptr, "test_find_missing_upstream");
}

void test_find_existing_upstream_by_upstream() {
    SessionRegistry session{};
    session.create(1);
    UniqueFd upstream_fd(2);
    session.bind_upstream(1, 2);
    expect(session.find_by_upstream(2)->client_fd == 1, "test_find_existing_upstream_by_upstream");
}

void test_find_missing_upstream_by_upstream() {
    SessionRegistry session{};
    session.create(1);
    UniqueFd upstream_fd(2);
    session.bind_upstream(1, 2);
    expect(session.find_by_upstream(3) == nullptr, "test_find_missing_upstream_by_upstream");
}

void test_erase_existing_client() {
    SessionRegistry session{};
    session.create(1);
    UniqueFd upstream_fd(2);
    session.bind_upstream(1, 2);
    expect(session.erase_by_client(1), "test_erase_existing_client");
}

void test_erase_missing_client() {
    SessionRegistry session{};
    session.create(1);
    UniqueFd upstream_fd(2);
    session.bind_upstream(1, 2);
    expect(!session.erase_by_client(2), "test_erase_existing_client");
}

void test_erase_missing_upstream() {
    SessionRegistry session{};
    session.create(1);
    UniqueFd upstream_fd(2);
    // session.bind_upstream(1, 2);
    expect(!session.erase_by_client(1), "test_erase_existing_client");
}

int main() {
    test_create_new_key();
    test_create_old_key();
    test_bind_new_key();
    test_bind_old_key();
    test_find_existing_upstream_by_client();
    test_find_missing_client_by_client();
    test_find_missing_upstream_by_client();
    test_find_existing_upstream_by_upstream();
    test_find_missing_upstream_by_upstream();
    test_erase_existing_client();
    test_erase_missing_client();
    test_erase_missing_upstream();
    std::cout << "All upstream connector tests passed.\n";
    return EXIT_SUCCESS;
}