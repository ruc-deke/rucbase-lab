// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include <cstdio>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include "net/client.h"

constexpr int kDefaultPort = 8765;

bool is_exit_command(std::string &cmd) { return cmd == "exit" || cmd == "exit;" || cmd == "bye" || cmd == "bye;"; }

void send_recv_sql(rucbase::wire::Client *client, const std::string &sql) {
    const size_t first = sql.find_first_not_of(" \t\r\n");
    if (first == std::string::npos || sql.compare(first, 2, "--") == 0) {
        return;
    }
    const rucbase::wire::ExecuteResult result = client->Execute(sql);
    if (!result.ok()) {
        std::cerr << "EXEC_STREAM failed: " << (result.diagnostic.empty() ? "unknown error" : result.diagnostic)
                  << '\n';
        std::cout << "failure\n";
        return;
    }
    if (!result.text.empty()) {
        std::cout << result.text;
        if (result.text.back() != '\n') {
            std::cout << '\n';
        }
    }
}

enum TestCase {
    TRANSACTION_COMMIT_TEST,
    TRANSACTION_ABORT_TEST
};

std::string test_infiles[] = {
    "transaction_test/commit_test.sql",
    "transaction_test/abort_test.sql"
};

std::string init_test_arguments(TestCase test_case) {
    return test_infiles[test_case];
}

rucbase::wire::Client connect_database(const char *unix_socket_path, const char *server_host,
                                       int server_port) {
    const rucbase::wire::Endpoint endpoint =
        unix_socket_path != nullptr
            ? rucbase::wire::Endpoint::Unix(unix_socket_path)
            : rucbase::wire::Endpoint::Tcp(server_host, static_cast<uint16_t>(server_port));
    rucbase::wire::Client client;
    const rucbase::wire::ClientStatus status = client.Connect(endpoint);
    if (!status.ok()) {
        std::cerr << status.message << '\n';
        std::exit(1);
    }
    return client;
}

int main(int argc, char *argv[]) {
    const char *unix_socket_path = nullptr;
    const char *server_host = "127.0.0.1";  // 127.0.0.1 192.168.31.25
    int server_port = kDefaultPort;
    int opt;

    while ((opt = getopt(argc, argv, "s:h:p:")) > 0) {
        switch (opt) {
            case 's':
                unix_socket_path = optarg;
                break;
            case 'p':
                char *ptr;
                server_port = (int)strtol(optarg, &ptr, 10);
                break;
            case 'h':
                server_host = optarg;
                break;
            default:
                break;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Test file needed.\n");
        return 1;
    }
    std::string test_name = argv[optind];

    // const char *prompt_str = "RUCBase > ";

    auto client = connect_database(unix_socket_path, server_host, server_port);

    std::ifstream test;
    std::string sql;
    
    // 测试点1
    test.open(test_name);
    while(std::getline(test, sql)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        send_recv_sql(&client, sql);
    }

    return 0;
}
