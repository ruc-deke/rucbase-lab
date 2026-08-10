// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "concurrency_test.h"
#include "../regress/regress_test.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>

int main(int argc, char* argv[]) {
    const char *unix_socket_path = nullptr;
    const char *server_host = "127.0.0.1";
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

    if (argc - optind < 1) {
        fprintf(stderr, "Test_case needed.\n");
        exit(1);
    }

    TestCaseAnalyzer analyzer;
    analyzer.infile_path = argv[optind];
    analyzer.analyze_test_case();

    auto preload_client = connect_database(unix_socket_path, server_host, server_port);
    for(size_t i = 0; i < analyzer.preload.size(); ++i) {
        if (!execute_sql(&preload_client, analyzer.preload[i]).ok()) {
            break;
        }
    }
    preload_client.Close();

    for(size_t i = 0; i < analyzer.transactions.size(); ++i) {
        analyzer.transactions[i]->client = connect_database(unix_socket_path, server_host, server_port);
    }

    const OperationPermutation& permutation = analyzer.permutation;
    for(size_t i = 0; i < permutation.operations.size(); ++i) {
        const auto transaction_index =
            static_cast<size_t>(permutation.operations[i]->txn_id);
        Transaction* txn = analyzer.transactions[transaction_index].get();
        const rucbase::wire::ExecuteResult result =
            execute_sql(&txn->client, permutation.operations[i]->sql);
        if (result.ok()) {
            std::cout << result.text;
        } else {
            std::cerr << "EXEC_STREAM failed: "
                      << (result.diagnostic.empty() ? "unknown error" : result.diagnostic) << '\n';
            std::cout << "failure\n";
        }
    }
    return 0;
}
