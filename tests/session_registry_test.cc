#include "session_registry.h"

#include "unique_fd.h"

#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <string_view>
#include <utility>

namespace {

void expect(bool condition, std::string_view test_name) {
    if (!condition) {
        std::cerr << "[FAILED] " << test_name << '\n';
        std::exit(EXIT_FAILURE);
    }
    std::cout << "[PASSED] " << test_name << '\n';
}

UniqueFd make_test_fd() {
    const int fd = open("/dev/null", O_RDWR);
    expect(fd >= 0, "open a real test fd");
    return UniqueFd(fd);
}

void test_create_and_find_client() {
    SessionRegistry registry;
    expect(registry.create(100), "create client session");
    expect(!registry.create(100), "reject duplicate client session");
    expect(!registry.create(-1), "reject invalid client fd");

    const auto* session = registry.find_by_client(100);
    expect(session != nullptr, "find created client session");
    expect(session->client_fd == 100, "created session stores client fd");
    expect(!session->upstream_fd.has_value(),
           "new session has no upstream fd");
}

void test_bind_and_find_upstream() {
    SessionRegistry registry;
    expect(registry.create(100), "create session before binding");

    auto upstream = make_test_fd();
    const int upstream_number = upstream.get();
    expect(registry.bind_upstream(100, std::move(upstream)),
           "bind upstream to client");
    expect(!upstream.valid(), "binding transfers upstream ownership");

    const auto* by_client = registry.find_by_client(100);
    expect(by_client != nullptr, "find bound session by client");
    expect(by_client->upstream_fd.has_value(),
           "bound session owns upstream fd");
    expect(by_client->upstream_fd->get() == upstream_number,
           "bound session stores the correct upstream fd");

    const auto* by_upstream = registry.find_by_upstream(upstream_number);
    expect(by_upstream == by_client,
           "client and upstream lookup return the same session");
}

void test_bind_rejects_invalid_requests_without_changing_registry() {
    SessionRegistry registry;
    expect(registry.create(100), "create session for rejected bindings");

    UniqueFd invalid_upstream;
    expect(!registry.bind_upstream(100, std::move(invalid_upstream)),
           "reject invalid upstream fd");
    expect(!registry.find_by_client(100)->upstream_fd.has_value(),
           "invalid bind leaves session unbound");

    auto missing_client_upstream = make_test_fd();
    expect(!registry.bind_upstream(999, std::move(missing_client_upstream)),
           "reject binding for missing client");
    expect(!registry.find_by_client(100)->upstream_fd.has_value(),
           "missing-client bind leaves existing session unchanged");
}

void test_second_bind_preserves_original_mapping() {
    SessionRegistry registry;
    expect(registry.create(100), "create session for duplicate bind");

    auto first_upstream = make_test_fd();
    const int first_number = first_upstream.get();
    expect(registry.bind_upstream(100, std::move(first_upstream)),
           "bind first upstream");

    auto second_upstream = make_test_fd();
    const int second_number = second_upstream.get();
    expect(!registry.bind_upstream(100, std::move(second_upstream)),
           "reject second upstream for the same client");
    expect(registry.find_by_upstream(first_number) != nullptr,
           "failed second bind preserves original reverse mapping");
    expect(registry.find_by_upstream(second_number) == nullptr,
           "failed second bind creates no reverse mapping");
}

void test_erase_unbound_session() {
    SessionRegistry registry;
    expect(registry.create(100), "create unbound session to erase");
    expect(registry.erase_by_client(100), "erase unbound client session");
    expect(registry.find_by_client(100) == nullptr,
           "unbound session is removed");
    expect(!registry.erase_by_client(100),
           "erasing a missing session reports false");
}

void test_unbind_upstream_preserves_client_session() {
    SessionRegistry registry;
    expect(registry.create(100), "create session before unbinding");

    auto upstream = make_test_fd();
    const int upstream_number = upstream.get();
    expect(registry.bind_upstream(100, std::move(upstream)),
           "bind upstream before unbinding");
    expect(registry.unbind_upstream(100), "unbind existing upstream");

    const auto* session = registry.find_by_client(100);
    expect(session != nullptr, "unbinding preserves client session");
    expect(!session->upstream_fd.has_value(),
           "unbinding clears upstream ownership");
    expect(registry.find_by_upstream(upstream_number) == nullptr,
           "unbinding clears reverse mapping");

    errno = 0;
    expect(fcntl(upstream_number, F_GETFD) == -1 && errno == EBADF,
           "unbinding closes upstream fd");
    expect(!registry.unbind_upstream(100),
           "unbinding an unbound session reports false");
}

void test_erase_bound_session_cleans_mapping_and_closes_fd() {
    SessionRegistry registry;
    expect(registry.create(100), "create bound session to erase");

    auto upstream = make_test_fd();
    const int upstream_number = upstream.get();
    expect(registry.bind_upstream(100, std::move(upstream)),
           "bind upstream before erasing session");
    expect(registry.erase_by_client(100), "erase bound client session");

    expect(registry.find_by_client(100) == nullptr,
           "bound session is removed");
    expect(registry.find_by_upstream(upstream_number) == nullptr,
           "reverse mapping is removed");

    errno = 0;
    expect(fcntl(upstream_number, F_GETFD) == -1 && errno == EBADF,
           "erasing session closes owned upstream fd");
}

}  // namespace

int main() {
    test_create_and_find_client();
    test_bind_and_find_upstream();
    test_bind_rejects_invalid_requests_without_changing_registry();
    test_second_bind_preserves_original_mapping();
    test_erase_unbound_session();
    test_unbind_upstream_preserves_client_session();
    test_erase_bound_session_cleans_mapping_and_closes_fd();
    std::cout << "All session registry tests passed.\n";
    return EXIT_SUCCESS;
}
