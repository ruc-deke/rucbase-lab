#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <termios.h>

#include <iostream>
#include <memory>
#include <fstream>

#include "regress_test.h"

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
    bzero(&(serv_addr.sin_zero), 8);

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
    int send_bytes;
    int recv_bytes;
    
    if((send_bytes = write(sockfd, sql.c_str(), sql.length() + 1)) == -1) {
        fprintf(stderr, "Send Error %d: %s\n", errno, strerror(errno));
        exit(1);
    }

    memset(recv_buf, 0, MAX_MEM_BUFFER_SIZE);
    recv_bytes = recv(sockfd, recv_buf, MAX_MEM_BUFFER_SIZE, 0);

    if(recv_bytes < 0) {
        fprintf(stderr, "Connection was broken: %s\n", strerror(errno));
        exit(1);
    }
    else if(recv_bytes == 0) {
        printf("Connection has been closed\n");
        exit(1);
    }

    return recv_bytes;
}

void start_test(int sockfd, std::string infile) {
    std::ifstream test_input;
    std::string sql;
    char recv_buf[MAX_MEM_BUFFER_SIZE];

    test_input.open(infile);
    
    while(std::getline(test_input, sql)) {
        memset(recv_buf, 0, sizeof(recv_buf));
        if(send_recv_sql(sockfd, sql, recv_buf) <= 0)
            break;
    }
}