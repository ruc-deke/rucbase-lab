#include "concurrency_test.h"
#include "../regress/regress_test.h"
#include <string.h>
#include <stdio.h>
#include <iostream>

int main(int argc, char* argv[]) {
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

    if (argc - optind < 1) {
        fprintf(stderr, "Test_case needed.\n");
        exit(1);
    }

    TestCaseAnalyzer* analyzer = new TestCaseAnalyzer();
    analyzer->infile_path = argv[optind];
    analyzer->analyze_test_case();

    int preload_sockfd = connect_database(unix_socket_path, server_host, server_port);
    for(size_t i = 0; i < analyzer->preload.size(); ++i) {
        if(send_sql(preload_sockfd, analyzer->preload[i]) < 0)
            break;
    }
    disconnect(preload_sockfd);

    for(size_t i = 0; i < analyzer->transactions.size(); ++i) {
        analyzer->transactions[i]->sockfd = connect_database(unix_socket_path, server_host, server_port);
    }

    OperationPermutation* permutation = analyzer->permutation;
    char recv_buf[MAX_MEM_BUFFER_SIZE];
    for(size_t i = 0; i < permutation->operations.size(); ++i) {
        Transaction* txn = analyzer->transactions[permutation->operations[i]->txn_id];
        const int response_size = send_recv_sql(txn->sockfd, permutation->operations[i]->sql, recv_buf);
        if (response_size > 0) {
            std::cout.write(recv_buf, response_size);
        }
    }

    for(size_t i = 0; i < analyzer->transactions.size(); ++i) {
        disconnect(analyzer->transactions[i]->sockfd);
    }
    return 0;
}
