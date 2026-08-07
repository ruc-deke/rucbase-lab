// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include <iostream>

#include "server/server.h"

namespace {

constexpr char kRucbaseBanner[] = R"(
  ____  _   _  ____ ____    _    ____  _____
 |  _ \| | | |/ ___| __ )  / \  / ___|| ____|
 | |_) | | | | |   |  _ \ / _ \ \___ \|  _|
 |  _ <| |_| | |___| |_) / ___ \ ___) | |___
 |_| \_ \___/ \____|____/_/   \_\____/|_____|

Welcome to Rucbase!
Type 'help;' for help.

)";

}  // namespace

int main(int argc, char** argv) {
    std::cout << kRucbaseBanner;
    return rucbase::Server::start(argc, argv);
}
