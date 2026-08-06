#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <termios.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <fstream>

#include "regress_test.h"
#include "net/wire.h"

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

int send_sql(int sockfd, const std::string& sql) {
    char recv_buf[MAX_MEM_BUFFER_SIZE];
    
    return send_recv_sql(sockfd, sql, recv_buf);
}
int send_recv_sql(int sockfd, const std::string& sql, char* recv_buf) {
    const size_t first = sql.find_first_not_of(" \t\r\n");
    if (first == std::string::npos || sql.compare(first, 2, "--") == 0) {
        memset(recv_buf, 0, MAX_MEM_BUFFER_SIZE);
        return 0;
    }
    std::string response;
    std::string diagnostic;
    if (!rucbase::wire::ExecStream(sockfd, sql, &response, &diagnostic)) {
        fprintf(stderr, "EXEC_STREAM failed: %s\n",
                diagnostic.empty() ? "unknown error" : diagnostic.c_str());
        const char *failure = "failure\n";
        memcpy(recv_buf, failure, strlen(failure) + 1);
        return static_cast<int>(strlen(failure));
    }

    memset(recv_buf, 0, MAX_MEM_BUFFER_SIZE);
    const size_t copy_n = std::min(response.size(), static_cast<size_t>(MAX_MEM_BUFFER_SIZE - 1));
    if (copy_n > 0) {
        memcpy(recv_buf, response.data(), copy_n);
    }
    return static_cast<int>(copy_n);
}

void start_test(int sockfd, std::string infile) {
    std::ifstream test_input;
    std::string sql;
    char recv_buf[MAX_MEM_BUFFER_SIZE];

    test_input.open(infile);
    
    while(std::getline(test_input, sql)) {
        memset(recv_buf, 0, sizeof(recv_buf));
        const int response_size = send_recv_sql(sockfd, sql, recv_buf);
        if (response_size > 0) {
            std::cout.write(recv_buf, response_size);
        }
    }
}
