// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include "net/client.h"
#include "typed_result.h"

constexpr int kDefaultPort = 8765;

rucbase::wire::Client connect_database(const char* unix_socket_path, const char* server_host, int server_port) {
    const rucbase::wire::Endpoint endpoint =
        unix_socket_path != nullptr ? rucbase::wire::Endpoint::Unix(unix_socket_path)
                                    : rucbase::wire::Endpoint::Tcp(server_host, static_cast<uint16_t>(server_port));
    rucbase::wire::Client client;
    const rucbase::wire::ClientStatus status = client.Connect(endpoint);
    if (!status.ok()) {
        std::cerr << status.message << '\n';
        std::exit(1);
    }
    return client;
}

int main(int argc, char* argv[]) {
    const char* unix_socket_path = nullptr;
    const char* server_host = "127.0.0.1";  // 127.0.0.1 192.168.31.25
    int server_port = kDefaultPort;
    int opt;

    while ((opt = getopt(argc, argv, "s:h:p:")) > 0) {
        switch (opt) {
            case 's':
                unix_socket_path = optarg;
                break;
            case 'p':
                char* ptr;
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
    while (std::getline(test, sql)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (!rucbase::test::ExecuteAndWriteTypedResult(&client, sql, std::cout, std::cerr)) {
            return 1;
        }
    }

    return 0;
}
