// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include <getopt.h>
#include <cstdio>
#include <readline/history.h>
#include <readline/readline.h>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "net/client.h"

namespace {

constexpr int kDefaultPort = 8765;

void PrintUsage(const char *prog) {
    std::cerr
        << "Usage: " << prog << " [options]\n"
        << "\n"
        << "Rucbase interactive SQL client (staff-provided).\n"
        << "\n"
        << "Options:\n"
        << "  -h <host>     Server host (default: 127.0.0.1)\n"
        << "  -p <port>     Server port (default: " << kDefaultPort << ")\n"
        << "  -e <sql>      Execute one statement and exit\n"
        << "  -f <file>     Execute SQL statements from a file and exit\n"
        << "  -q            Quiet mode (suppress the welcome banner)\n"
        << "  -?, --help    Show this help\n"
        << "\n"
        << "Interactive tips:\n"
        << "  - End a statement with ';'\n"
        << "  - Multi-line input is supported until ';'\n"
        << "  - Type exit; or bye; (or Ctrl-D) to quit\n"
        << "\n"
        << "Examples:\n"
        << "  " << prog << "\n"
        << "  " << prog << " -h 127.0.0.1 -p 8765\n"
        << "  " << prog << " -e \"show tables;\"\n"
        << "  " << prog << " -f demo.sql\n";
}

bool IsExitCommand(const std::string &cmd) {
    return cmd == "exit" || cmd == "exit;" || cmd == "bye" || cmd == "bye;";
}

std::string Trim(const std::string &input) {
    const auto begin = input.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    const auto end = input.find_last_not_of(" \t\r\n");
    return input.substr(begin, end - begin + 1);
}

bool LooksComplete(const std::string &sql) {
    const std::string trimmed = Trim(sql);
    return !trimmed.empty() && trimmed.back() == ';';
}

struct SendResult {
    bool statement_ok = false;
    bool connection_reusable = false;
};

SendResult SendSql(rucbase::wire::Client *client, const std::string &sql, bool print_response) {
    const std::string command = Trim(sql);
    if (command.empty()) {
        return {true, true};
    }

    rucbase::wire::ExecuteResult result = client->Execute(command);
    if (!result.ok()) {
        if (!result.diagnostic.empty()) {
            std::cerr << result.diagnostic;
            if (result.diagnostic.back() != '\n') {
                std::cerr << '\n';
            }
        } else {
            std::cerr << "EXEC_STREAM failed\n";
        }
        return {false, result.connection_reusable()};
    }
    if (print_response && !result.text.empty()) {
        std::cout << result.text;
        if (result.text.back() != '\n') {
            std::cout << '\n';
        }
    }
    return {true, true};
}

std::string FetchDatabaseName(rucbase::wire::Client *client) {
    rucbase::wire::ExecuteResult result = client->Execute(rucbase::wire::kDatabaseNameRequest);
    if (!result.ok() || Trim(result.text).empty()) {
        return "?";
    }
    return Trim(result.text);
}

std::vector<std::string> SplitStatements(const std::string &script) {
    std::vector<std::string> statements;
    std::string current;
    bool in_string = false;
    bool in_line_comment = false;
    bool in_block_comment = false;

    const auto append_space = [&current]() {
        if (!current.empty() && current.back() != ' ') {
            current.push_back(' ');
        }
    };

    for (size_t index = 0; index < script.size(); ++index) {
        const char ch = script[index];
        const char next = index + 1 < script.size() ? script[index + 1] : '\0';

        if (in_line_comment) {
            if (ch == '\n') {
                in_line_comment = false;
                append_space();
            }
            continue;
        }

        if (in_block_comment) {
            if (ch == '*' && next == '/') {
                in_block_comment = false;
                ++index;
                append_space();
            }
            continue;
        }

        if (!in_string && ch == '-' && next == '-') {
            in_line_comment = true;
            ++index;
            append_space();
            continue;
        }
        if (!in_string && ch == '/' && next == '*') {
            in_block_comment = true;
            ++index;
            append_space();
            continue;
        }

        if (ch == '\'') {
            current.push_back(ch);
            if (in_string && next == '\'') {
                current.push_back(next);
                ++index;
            } else {
                in_string = !in_string;
            }
            continue;
        }

        if (!in_string && ch == ';') {
            current.push_back(ch);
            statements.push_back(Trim(current));
            current.clear();
            continue;
        }

        if (!in_string && (ch == '\n' || ch == '\r' || ch == '\t')) {
            append_space();
        } else {
            current.push_back(ch);
        }
    }

    if (!Trim(current).empty()) {
        statements.push_back(Trim(current));
    }
    return statements;
}

bool RunScriptFile(rucbase::wire::Client *client, const std::string &path) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "failed to open SQL file: " << path << '\n';
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    for (const auto &stmt : SplitStatements(buffer.str())) {
        if (IsExitCommand(stmt)) {
            return true;
        }
        if (!SendSql(client, stmt, true).statement_ok) {
            return false;
        }
    }
    return true;
}

int RunInteractive(rucbase::wire::Client *client, const std::string &database_name) {
    std::string pending;
    const std::string primary_prompt = "Rucbase(" + database_name + ")> ";
    const std::string continuation_prompt(primary_prompt.size() - 3, ' ');
    while (true) {
        const std::string prompt = pending.empty() ? primary_prompt : continuation_prompt + "-> ";
        char *line_read = readline(prompt.c_str());
        if (line_read == nullptr) {
            std::cout << "\n";
            break;
        }
        std::string line = line_read;
        free(line_read);

        if (Trim(line).empty() && pending.empty()) {
            continue;
        }

        if (!pending.empty()) {
            pending.push_back(' ');
        }
        pending += line;

        if (!LooksComplete(pending) && !IsExitCommand(Trim(pending))) {
            continue;
        }

        const std::string command = Trim(pending);
        pending.clear();
        if (command.empty()) {
            continue;
        }

        add_history(command.c_str());
        if (IsExitCommand(command)) {
            std::cout << "The client will be closed.\n";
            break;
        }
        const SendResult send_result = SendSql(client, command, true);
        if (!send_result.statement_ok && !send_result.connection_reusable) {
            return 1;
        }
    }
    return 0;
}

}  // namespace

int main(int argc, char *argv[]) {
    const char *server_host = "127.0.0.1";
    int server_port = kDefaultPort;
    const char *execute_sql = nullptr;
    const char *script_file = nullptr;
    bool quiet = false;

    opterr = 0;
    constexpr int kHelpOption = 1000;
    const option long_options[] = {
        {"help", no_argument, nullptr, kHelpOption},
        {nullptr, 0, nullptr, 0},
    };
    int opt = 0;
    while ((opt = getopt_long(argc, argv, ":h:p:e:f:q?", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'h':
                server_host = optarg;
                break;
            case 'p': {
                char *end = nullptr;
                const long value = std::strtol(optarg, &end, 10);
                if (end == optarg || *end != '\0' || value <= 0 || value > 65535) {
                    std::cerr << "invalid port: " << optarg << '\n';
                    return 1;
                }
                server_port = static_cast<int>(value);
                break;
            }
            case 'e':
                execute_sql = optarg;
                break;
            case 'f':
                script_file = optarg;
                break;
            case 'q':
                quiet = true;
                break;
            case kHelpOption:
                PrintUsage(argv[0]);
                return 0;
            case ':':
                std::cerr << "option requires an argument: -" << static_cast<char>(optopt) << '\n';
                PrintUsage(argv[0]);
                return 1;
            case '?':
                if (std::strcmp(argv[optind - 1], "-?") == 0) {
                    PrintUsage(argv[0]);
                    return 0;
                }
                if (optopt != 0) {
                    std::cerr << "unknown option: -" << static_cast<char>(optopt) << '\n';
                } else {
                    std::cerr << "unknown option: " << argv[optind - 1] << '\n';
                }
                PrintUsage(argv[0]);
                return 1;
            default:
                return 1;
        }
    }

    if (optind < argc) {
        std::cerr << "unexpected argument: " << argv[optind] << '\n';
        PrintUsage(argv[0]);
        return 1;
    }

    if (execute_sql != nullptr && script_file != nullptr) {
        std::cerr << "-e and -f cannot be used together\n";
        return 1;
    }

    rucbase::wire::Client client;
    const rucbase::wire::ClientStatus connect_status =
        client.ConnectTcp(server_host, static_cast<uint16_t>(server_port));
    if (!connect_status.ok()) {
        std::cerr << connect_status.message << "\n"
                  << "Hint: start the server first, e.g.\n"
                  << "  ./bin/rmdb -p " << server_port << " <database_name>\n";
        return 1;
    }

    const bool interactive = execute_sql == nullptr && script_file == nullptr;
    const std::string database_name = interactive ? FetchDatabaseName(&client) : "";

    if (!quiet && interactive) {
        std::cout << "\n"
                     "  ____  _   _  ____ ____    _    ____  _____ \n"
                     " |  _ \\| | | |/ ___| __ )  / \\  / ___|| ____|\n"
                     " | |_) | | | | |   |  _ \\ / _ \\ \\___ \\|  _|  \n"
                     " |  _ <| |_| | |___| |_) / ___ \\ ___) | |___ \n"
                     " |_| \\_ \\___/ \\____|____/_/   \\_\\____/|_____|\n"
                     "\n";
        std::cout << "Connected to " << server_host << ":" << server_port << "\n";
        std::cout << "Database: " << database_name << "\n";
        std::cout << "Type 'help;' for server help, 'exit;' to quit.\n\n";
    }

    int exit_code = 0;
    if (execute_sql != nullptr) {
        if (!IsExitCommand(Trim(execute_sql)) && !SendSql(&client, execute_sql, true).statement_ok) {
            exit_code = 1;
        }
    } else if (script_file != nullptr) {
        if (!RunScriptFile(&client, script_file)) {
            exit_code = 1;
        }
    } else {
        exit_code = RunInteractive(&client, database_name);
    }

    if (!quiet && interactive) {
        std::cout << "Bye.\n";
    }
    return exit_code;
}
