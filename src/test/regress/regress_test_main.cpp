// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include <stdlib.h>
#include <unistd.h>

#include "regress_test.h"

int main(int argc, char** argv) {
    const char* unix_socket_path = nullptr;
    const char* server_host = "127.0.0.1";
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
        fprintf(stderr, "Test_case needed.\n");
        exit(1);
    }
    std::string infile = argv[optind];

    auto client = connect_database(unix_socket_path, server_host, server_port);
    return start_test(&client, infile) ? 0 : 1;
}
