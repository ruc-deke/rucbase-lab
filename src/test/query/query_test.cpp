// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "net/client.h"
#include "typed_result.h"

constexpr int kDefaultPort = 8765;

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

    const rucbase::wire::Endpoint endpoint =
        unix_socket_path != nullptr ? rucbase::wire::Endpoint::Unix(unix_socket_path)
                                    : rucbase::wire::Endpoint::Tcp(server_host, static_cast<uint16_t>(server_port));
    rucbase::wire::Client client;
    const rucbase::wire::ClientStatus connect_status = client.Connect(endpoint);
    if (!connect_status.ok()) {
        std::cerr << connect_status.message << '\n';
        return 1;
    }

    std::ifstream test;
    std::string sql;

    // 测试点1
    test.open(test_name);
    while (std::getline(test, sql)) {
        if (!rucbase::test::ExecuteAndWriteTypedResult(&client, sql, std::cout, std::cerr)) {
            return 1;
        }
    }

    return 0;
}
