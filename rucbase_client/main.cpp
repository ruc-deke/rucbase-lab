#include <netdb.h>
#include <getopt.h>
#include <cstdio>
#include <readline/history.h>
#include <readline/readline.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "net/wire.h"

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

int InitTcpSocket(const char *server_host, int server_port) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo *result = nullptr;
    const std::string port = std::to_string(server_port);
    const int rc = getaddrinfo(server_host, port.c_str(), &hints, &result);
    if (rc != 0) {
        std::cerr << "getaddrinfo(" << server_host << ":" << server_port << ") failed: " << gai_strerror(rc)
                  << '\n';
        return -1;
    }

    int sockfd = -1;
    for (addrinfo *rp = result; rp != nullptr; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd < 0) {
            continue;
        }
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }
        close(sockfd);
        sockfd = -1;
    }
    freeaddrinfo(result);

    if (sockfd < 0) {
        std::cerr << "failed to connect to " << server_host << ":" << server_port << " (" << strerror(errno)
                  << ")\n"
                  << "Hint: start the server first, e.g.\n"
                  << "  ./bin/rmdb -p " << server_port << " <database_name>\n";
        return -1;
    }
    return sockfd;
}

bool SendSql(int sockfd, const std::string &sql, bool print_response) {
    const std::string command = Trim(sql);
    if (command.empty()) {
        return true;
    }

    std::string response;
    std::string diagnostic;
    if (!rucbase::wire::ExecStream(sockfd, command, &response, &diagnostic)) {
        if (!diagnostic.empty()) {
            std::cerr << diagnostic;
            if (diagnostic.back() != '\n') {
                std::cerr << '\n';
            }
        } else {
            std::cerr << "EXEC_STREAM failed\n";
        }
        return false;
    }
    if (print_response && !response.empty()) {
        std::cout << response;
        if (response.back() != '\n') {
            std::cout << '\n';
        }
    }
    return true;
}

std::string FetchDatabaseName(int sockfd) {
    std::string database;
    std::string diagnostic;
    if (!rucbase::wire::ExecStream(sockfd, rucbase::wire::kDatabaseNameRequest, &database, &diagnostic) ||
        Trim(database).empty()) {
        return "?";
    }
    return Trim(database);
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

bool RunScriptFile(int sockfd, const std::string &path) {
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
        if (!SendSql(sockfd, stmt, true)) {
            return false;
        }
    }
    return true;
}

int RunInteractive(int sockfd, const std::string &database_name) {
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
        if (!SendSql(sockfd, command, true)) {
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

    // Convert a peer-closed connection into an EPIPE error from send() instead
    // of terminating the client process.
    std::signal(SIGPIPE, SIG_IGN);

    const int sockfd = InitTcpSocket(server_host, server_port);
    if (sockfd < 0) {
        return 1;
    }

    if (!rucbase::wire::ClientHandshake(sockfd)) {
        std::cerr << "wire handshake failed (server must speak docs/rmdb_wire.md v3.0)\n";
        close(sockfd);
        return 1;
    }

    const bool interactive = execute_sql == nullptr && script_file == nullptr;
    const std::string database_name = interactive ? FetchDatabaseName(sockfd) : "";

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
        if (!IsExitCommand(Trim(execute_sql)) && !SendSql(sockfd, execute_sql, true)) {
            exit_code = 1;
        }
    } else if (script_file != nullptr) {
        if (!RunScriptFile(sockfd, script_file)) {
            exit_code = 1;
        }
    } else {
        exit_code = RunInteractive(sockfd, database_name);
    }

    close(sockfd);
    if (!quiet && interactive) {
        std::cout << "Bye.\n";
    }
    return exit_code;
}
