// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <ostream>

namespace rucbase {

namespace banner {

inline constexpr char kRed[] = "\033[1;31m";
inline constexpr char kBlue[] = "\033[1;34m";
inline constexpr char kReset[] = "\033[0m";

}  // namespace banner

/** @brief Print the shared RUCBase logo used by the server and interactive client. */
inline void PrintRucbaseBanner(std::ostream& output) {
    output << '\n'
           << banner::kRed << "██████╗ ██╗   ██╗ ██████╗" << banner::kBlue << " ██████╗  █████╗ ███████╗███████╗\n"
           << banner::kRed << "██╔══██╗██║   ██║██╔════╝" << banner::kBlue << " ██╔══██╗██╔══██╗██╔════╝██╔════╝\n"
           << banner::kRed << "██████╔╝██║   ██║██║     " << banner::kBlue << " ██████╔╝███████║███████╗█████╗  \n"
           << banner::kRed << "██╔══██╗██║   ██║██║     " << banner::kBlue << " ██╔══██╗██╔══██║╚════██║██╔══╝  \n"
           << banner::kRed << "██║  ██║╚██████╔╝╚██████╗" << banner::kBlue << " ██████╔╝██║  ██║███████║███████╗\n"
           << banner::kRed << "╚═╝  ╚═╝ ╚═════╝  ╚═════╝" << banner::kBlue << " ╚═════╝ ╚═╝  ╚═╝╚══════╝╚══════╝\n"
           << banner::kReset << '\n';
}

}  // namespace rucbase
