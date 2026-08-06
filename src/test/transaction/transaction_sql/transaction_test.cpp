#include <netdb.h>
#include <netinet/in.h>
#include <cstdio>
#include <readline/history.h>
#include <readline/readline.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <termios.h>
#include <unistd.h>

#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <fstream>
#include <mutex>

#include "net/wire.h"

#define MAX_MEM_BUFFER_SIZE 8192
#define PORT_DEFAULT 8765
#define MAX_CLIENT_NUM 4    // 同时连接服务端的客户端数量
#define MAX_TXN_PER_CLIENT 1000 // 每个客户端执行的事务数量

bool is_exit_command(std::string &cmd) { return cmd == "exit" || cmd == "exit;" || cmd == "bye" || cmd == "bye;"; }

int init_unix_sock(const char *unix_sock_path) {
    int sockfd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "failed to create unix socket. %s", strerror(errno));
        return -1;
    }

    struct sockaddr_un sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sun_family = PF_UNIX;
    snprintf(sockaddr.sun_path, sizeof(sockaddr.sun_path), "%s", unix_sock_path);

    if (connect(sockfd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) {
        fprintf(stderr, "failed to connect to server. unix socket path '%s'. error %s", sockaddr.sun_path,
                strerror(errno));
        close(sockfd);
        return -1;
    }
    return sockfd;
}

int init_tcp_sock(const char *server_host, int server_port) {
    struct hostent *host;
    struct sockaddr_in serv_addr;

    if ((host = gethostbyname(server_host)) == NULL) {
        fprintf(stderr, "gethostbyname failed. errmsg=%d:%s\n", errno, strerror(errno));
        return -1;
    }

    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "create socket error. errmsg=%d:%s\n", errno, strerror(errno));
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);
    serv_addr.sin_addr = *((struct in_addr *)host->h_addr);
    memset(&(serv_addr.sin_zero), 0, sizeof(serv_addr.sin_zero));

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) == -1) {
        fprintf(stderr, "Failed to connect. errmsg=%d:%s\n", errno, strerror(errno));
        close(sockfd);
        return -1;
    }
    return sockfd;
}

void send_recv_sql(int sockfd, std::string sql) {
    const size_t first = sql.find_first_not_of(" \t\r\n");
    if (first == std::string::npos || sql.compare(first, 2, "--") == 0) {
        return;
    }
    std::string response;
    std::string diagnostic;
    if (!rucbase::wire::ExecStream(sockfd, sql, &response, &diagnostic)) {
        std::cerr << "EXEC_STREAM failed: "
                  << (diagnostic.empty() ? "unknown error" : diagnostic) << std::endl;
        std::cout << "failure\n";
        return;
    }
    if (!response.empty()) {
        std::cout << response;
        if (response.back() != '\n') {
            std::cout << '\n';
        }
    }
}

enum TestCase {
    TRANSACTION_COMMIT_TEST,
    TRANSACTION_ABORT_TEST
};

std::string test_infiles[] = {
    "transaction_test/commit_test.sql",
    "transaction_test/abort_test.sql"
};

std::string init_test_arguments(TestCase test_case) {
    return test_infiles[test_case];
}

// 返回sockfd
int connect_database(const char* unix_sockect_path, const char* server_host, int server_port) {
    int sockfd;
    
    if(unix_sockect_path != nullptr) {
        sockfd = init_unix_sock(unix_sockect_path);
    }
    else {
        sockfd = init_tcp_sock(server_host, server_port);
    }

    if(sockfd < 0) {
        exit(1);
    }

    if (!rucbase::wire::ClientHandshake(sockfd)) {
        fprintf(stderr, "wire handshake failed\n");
        close(sockfd);
        exit(1);
    }

    return sockfd;
}

void disconnect(int sockfd) {
    close(sockfd);
}

void start_test() {
    
}


int main(int argc, char *argv[]) {
    int ret = 0;  // set_terminal_noncanonical();
                  //    if (ret < 0) {
                  //        printf("Warning: failed to set terminal non canonical. Long command may be "
                  //               "handled incorrect\n");
                  //    }

    const char *unix_socket_path = nullptr;
    const char *server_host = "127.0.0.1";  // 127.0.0.1 192.168.31.25
    int server_port = PORT_DEFAULT;
    int opt;
    std::string test_name = argv[1];
    

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

    // const char *prompt_str = "RucBase > ";

    int sockfd = connect_database(unix_socket_path, server_host, server_port);

    std::ifstream test;
    std::string sql;
    
    // 测试点1
    test.open(test_name);
    while(std::getline(test, sql)) {
        usleep(10000);
        send_recv_sql(sockfd, sql);
    }

    disconnect(sockfd);

    return 0;
}
