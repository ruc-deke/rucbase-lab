#include <stdlib.h>

#include "regress_test.h"

int main(int argc, char** argv) {
    if(argc < 2) {
        fprintf(stderr, "Test_case needed.\n");
        exit(1);
    }
    std::string infile = argv[1];

    const char *unix_socket_path = nullptr;
    const char *server_host = "127.0.0.1";
    int server_port = PORT_DEFAULT;
    int opt;

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
    
    int sockfd = connect_database(unix_socket_path, server_host, server_port);
    start_test(sockfd, infile);
    disconnect(sockfd);
}