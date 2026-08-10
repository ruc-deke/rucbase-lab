// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include <getopt.h>

#include <cctype>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "common/banner.h"
#include "net/client.h"

namespace {

constexpr int kDefaultPort = 8765;

void PrintUsage(std::ostream& output, const char* prog) {
    output << "Usage: " << prog << " [options]\n"
           << "\n"
           << "RUCBase interactive SQL client (staff-provided).\n"
           << "\n"
           << "Options:\n"
           << "  -h <host>     Server host (default: 127.0.0.1)\n"
           << "  -p <port>     Server port (default: " << kDefaultPort << ")\n"
           << "  -e <sql>      Execute SQL and exit\n"
           << "  -f <file>     Execute SQL statements from a file and exit\n"
           << "  --help        Show this help\n"
           << "\n"
           << "Interactive tips:\n"
           << "  - End a statement with ';'\n"
           << "  - Multi-line and multi-statement input are supported\n"
           << "  - Type exit; or bye; (or Ctrl-D) to quit\n"
           << "\n"
           << "Examples:\n"
           << "  " << prog << "\n"
           << "  " << prog << " -h 127.0.0.1 -p 8765\n"
           << "  " << prog << " -e \"show tables;\"\n"
           << "  " << prog << " -f demo.sql\n";
}

std::string Trim(const std::string& input) {
    const auto begin = input.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    const auto end = input.find_last_not_of(" \t\r\n");
    return input.substr(begin, end - begin + 1);
}

// Take semicolon-terminated statements and keep the unfinished tail in input.
std::vector<std::string> TakeCompleteStatements(std::string& input) {
    std::vector<std::string> statements;
    std::size_t statement_start = 0;
    bool in_string = false;
    bool in_line_comment = false;
    bool in_block_comment = false;
    bool tail_has_sql = false;

    for (std::size_t index = 0; index < input.size(); ++index) {
        const char ch = input[index];
        const char next = index + 1 < input.size() ? input[index + 1] : '\0';

        if (in_line_comment) {
            if (ch == '\n') {
                in_line_comment = false;
            }
            continue;
        }
        if (in_block_comment) {
            if (ch == '*' && next == '/') {
                in_block_comment = false;
                ++index;
            }
            continue;
        }
        if (in_string) {
            if (ch == '\'') {
                if (next == '\'') {
                    ++index;
                } else {
                    in_string = false;
                }
            }
            continue;
        }

        if (ch == '-' && next == '-') {
            in_line_comment = true;
            ++index;
        } else if (ch == '/' && next == '*') {
            in_block_comment = true;
            ++index;
        } else if (ch == '\'') {
            in_string = true;
            tail_has_sql = true;
        } else if (ch == ';') {
            if (tail_has_sql) {
                statements.push_back(input.substr(statement_start, index - statement_start + 1));
            }
            statement_start = index + 1;
            tail_has_sql = false;
        } else if (!std::isspace(static_cast<unsigned char>(ch))) {
            tail_has_sql = true;
        }
    }

    input.erase(0, statement_start);
    if (!tail_has_sql && !in_block_comment) {
        input.clear();
    }
    return statements;
}

bool IsExitCommand(const std::string& command) {
    const std::string trimmed = Trim(command);
    return trimmed == "exit" || trimmed == "exit;" || trimmed == "bye" || trimmed == "bye;";
}

rucbase::wire::ExecuteResult SendSql(rucbase::wire::Client& client,
                                     const std::string& sql,
                                     const bool highlight_errors = false) {
    const std::string command = Trim(sql);
    rucbase::wire::ExecuteResult result = client.Execute(command);
    if (!result.ok()) {
        if (highlight_errors) {
            std::cout << "\033[1;31m";
        }
        if (!result.diagnostic.empty()) {
            std::cout << result.diagnostic;
            if (result.diagnostic.back() != '\n') {
                std::cout << '\n';
            }
        } else {
            std::cout << "EXEC_STREAM failed\n";
        }
        if (highlight_errors) {
            std::cout << "\033[0m";
        }
        std::cout.flush();
        return result;
    }
    if (result.status == rucbase::wire::ExecuteStatus::CommandOk) {
        std::cout << "Query OK\n";
        std::cout.flush();
    } else if (!result.text.empty()) {
        std::cout << result.text;
        if (result.text.back() != '\n') {
            std::cout << '\n';
        }
        std::cout.flush();
    }
    return result;
}

std::string FetchDatabaseName(rucbase::wire::Client& client) {
    std::string database_name;
    rucbase::wire::ExecuteOptions options;
    options.format_text = false;
    options.on_row = [&database_name](const std::vector<rucbase::wire::Cell>& row) {
        if (row.size() == 1 && !row[0].is_null && row[0].sql_type == rucbase::wire::kTypeChar) {
            database_name = row[0].str_val;
        }
        return true;
    };

    const rucbase::wire::ExecuteResult result = client.Execute("show database;", options);
    if (!result.ok() || result.row_count != 1 || result.columns.size() != 1 ||
        result.columns[0].sql_type != rucbase::wire::kTypeChar) {
        return "?";
    }
    const std::string name = Trim(database_name);
    return name.empty() ? "?" : name;
}

bool RunSqlText(rucbase::wire::Client& client, std::string sql) {
    std::vector<std::string> statements = TakeCompleteStatements(sql);
    if (!Trim(sql).empty()) {
        statements.push_back(Trim(sql));
    }

    for (const std::string& statement : statements) {
        if (IsExitCommand(statement)) {
            return true;
        }
        if (!SendSql(client, statement).ok()) {
            return false;
        }
    }
    return true;
}

bool RunScriptFile(rucbase::wire::Client& client, const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "failed to open SQL file: " << path << '\n';
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return RunSqlText(client, buffer.str());
}

int RunInteractive(rucbase::wire::Client& client, const std::string& database_name) {
    std::string pending;
    const std::string primary_prompt = "RUCBase(" + database_name + ")> ";
    const std::string continuation_prompt(primary_prompt.size() - 3, ' ');

    while (true) {
        const std::string prompt = pending.empty() ? primary_prompt : continuation_prompt + "-> ";
        std::cout << prompt << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) {
            std::cout << '\n';
            if (!Trim(pending).empty()) {
                std::cerr << "incomplete SQL statement discarded\n";
            }
            return 0;
        }

        pending += line;
        pending.push_back('\n');

        if (IsExitCommand(pending)) {
            std::cout << "The client will be closed.\n";
            return 0;
        }

        for (const std::string& statement : TakeCompleteStatements(pending)) {
            if (IsExitCommand(statement)) {
                std::cout << "The client will be closed.\n";
                return 0;
            }
            if (const auto result = SendSql(client, statement, true); !result.ok() && !result.connection_reusable()) {
                return 1;
            }
        }
    }
}

}  // namespace

namespace {

/** @brief 解析参数并运行客户端；异常由 main 统一转换为诊断信息。 */
int RunClient(int argc, char* argv[]) {
    const auto* server_host = "127.0.0.1";
    int server_port = kDefaultPort;
    const char* execute_sql = nullptr;
    const char* script_file = nullptr;

    opterr = 0;
    constexpr int kHelpOption = 1000;
    constexpr option long_options[] = {
        {.name = "help", .has_arg = no_argument, .flag = nullptr, .val = kHelpOption},
        {.name = nullptr, .has_arg = 0, .flag = nullptr, .val = 0},
    };
    int opt = 0;
    while ((opt = getopt_long(argc, argv, ":h:p:e:f:", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'h':
                server_host = optarg;
                break;
            case 'p': {
                char* end = nullptr;
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
            case kHelpOption:
                PrintUsage(std::cout, argv[0]);
                return 0;
            case ':':
                std::cerr << "option requires an argument: -" << static_cast<char>(optopt) << '\n';
                PrintUsage(std::cerr, argv[0]);
                return 1;
            case '?':
                if (optopt != 0) {
                    std::cerr << "unknown option: -" << static_cast<char>(optopt) << '\n';
                } else {
                    std::cerr << "unknown option: " << argv[optind - 1] << '\n';
                }
                PrintUsage(std::cerr, argv[0]);
                return 1;
            default:
                return 1;
        }
    }

    if (optind < argc) {
        std::cerr << "unexpected argument: " << argv[optind] << '\n';
        PrintUsage(std::cerr, argv[0]);
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
    const std::string database_name = interactive ? FetchDatabaseName(client) : "";

    if (interactive) {
        rucbase::PrintRucbaseBanner(std::cout);
        std::cout << "Connected to " << server_host << ":" << server_port << "\n";
        std::cout << "Database: " << database_name << "\n";
        std::cout << "Type 'help;' for server help, 'exit;' to quit.\n\n";
    }

    int exit_code = 0;
    if (execute_sql != nullptr) {
        if (!RunSqlText(client, execute_sql)) {
            exit_code = 1;
        }
    } else if (script_file != nullptr) {
        if (!RunScriptFile(client, script_file)) {
            exit_code = 1;
        }
    } else {
        exit_code = RunInteractive(client, database_name);
    }

    if (interactive) {
        std::cout << "Bye.\n";
    }
    return exit_code;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        return RunClient(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "client error: " << error.what() << '\n';
        return 1;
    }
}
