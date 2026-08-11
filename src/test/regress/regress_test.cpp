// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "regress_test.h"

#include <cstdlib>
#include <fstream>
#include <iostream>

#include "typed_result.h"

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

rucbase::wire::ExecuteResult execute_sql(rucbase::wire::Client* client, const std::string& sql) {
    return rucbase::test::ExecuteWithoutText(client, sql);
}

bool start_test(rucbase::wire::Client* client, const std::string& infile) {
    std::ifstream test_input;
    std::string sql;

    test_input.open(infile);

    while (std::getline(test_input, sql)) {
        if (!rucbase::test::ExecuteAndWriteTypedResult(client, sql, std::cout, std::cerr)) {
            return false;
        }
    }
    return true;
}
