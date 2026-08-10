// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include <iostream>

#include "common/banner.h"
#include "server/server.h"

int main(int argc, char** argv) {
    rucbase::PrintRucbaseBanner(std::cout);
    std::cout << "Welcome to RUCBase!\n"
              << "Start the client: rucbase_client -p <port>\n\n"
              << std::flush;
    return rucbase::Server::start(argc, argv);
}
