// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include <cstdlib>
#include <fstream>
#include <iostream>

#include "regress_test.h"

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

rucbase::wire::ExecuteResult execute_sql(rucbase::wire::Client *client, const std::string &sql) {
    const size_t first = sql.find_first_not_of(" \t\r\n");
    if (first == std::string::npos || sql.compare(first, 2, "--") == 0) {
        rucbase::wire::ExecuteResult result;
        result.status = rucbase::wire::ExecuteStatus::CommandOk;
        return result;
    }
    return client->Execute(sql);
}

void start_test(rucbase::wire::Client *client, const std::string &infile) {
    std::ifstream test_input;
    std::string sql;

    test_input.open(infile);

    while (std::getline(test_input, sql)) {
        const rucbase::wire::ExecuteResult result = execute_sql(client, sql);
        if (result.ok()) {
            std::cout << result.text;
        } else {
            std::cerr << "EXEC_STREAM failed: "
                      << (result.diagnostic.empty() ? "unknown error" : result.diagnostic) << '\n';
            std::cout << "failure\n";
        }
    }
}
