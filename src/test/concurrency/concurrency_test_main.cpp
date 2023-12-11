#include "concurrency_test.h"
#include "../regress/regress_test.h"
#include <string.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
    if(argc < 3) {
        fprintf(stderr, "Test_case and outfile_path needed.\n");
        exit(1);
    }

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

    TestCaseAnalyzer* analyzer = new TestCaseAnalyzer();
    std::string outfile_path = argv[2];
    std::fstream outfile;
    outfile.open(outfile_path, std::ios::out | std::ios::trunc);
    analyzer->infile_path = argv[1];
    analyzer->analyze_test_case();

    int preload_sockfd = connect_database(unix_socket_path, server_host, server_port);
    for(size_t i = 0; i < analyzer->preload.size(); ++i) {
        if(send_sql(preload_sockfd, analyzer->preload[i]) <= 0)
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
        send_recv_sql(txn->sockfd, permutation->operations[i]->sql, recv_buf);
        outfile << recv_buf;
    }

    outfile.close();

    for(size_t i = 0; i < analyzer->transactions.size(); ++i) {
        disconnect(analyzer->transactions[i]->sockfd);
    }
    return 0;
}