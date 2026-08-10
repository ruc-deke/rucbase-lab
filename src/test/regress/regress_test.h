// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <string>

#include "net/client.h"
#include "net/wire.h"

constexpr int kDefaultPort = 8765;

rucbase::wire::Client connect_database(const char *unix_socket_path, const char *server_host,
                                       int server_port);
rucbase::wire::ExecuteResult execute_sql(rucbase::wire::Client *client, const std::string &sql);
void start_test(rucbase::wire::Client *client, const std::string &infile);
