#include <netinet/in.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>

#include "errors.h"
#include "interp.h"
#include "recovery/log_recovery.h"

#define SOCK_PORT 8765
#define BUFFER_LENGTH 8192
#define MAX_CONN_LIMIT 8

static bool should_exit = false;

auto disk_manager = std::make_unique<DiskManager>();
auto buffer_pool_manager = std::make_unique<BufferPoolManager>(BUFFER_POOL_SIZE, disk_manager.get());
auto rm_manager = std::make_unique<RmManager>(disk_manager.get(), buffer_pool_manager.get());
auto ix_manager = std::make_unique<IxManager>(disk_manager.get(), buffer_pool_manager.get());
auto sm_manager =
    std::make_unique<SmManager>(disk_manager.get(), buffer_pool_manager.get(), rm_manager.get(), ix_manager.get());
auto ql_manager = std::make_unique<QlManager>(sm_manager.get());
auto lock_manager = std::make_unique<LockManager>();
auto txn_manager = std::make_unique<TransactionManager>(lock_manager.get(), sm_manager.get());
auto log_manager = std::make_unique<LogManager>(disk_manager.get());
auto interp = std::make_unique<Interp>(sm_manager.get(), ql_manager.get(), txn_manager.get());
auto recovery = std::make_unique<LogRecovery>(sm_manager.get(), disk_manager.get());

static jmp_buf jmpbuf;
void sigint_handler(int signo) {
    should_exit = true;
    if (log_manager->GetLogMode()) {
        log_manager->StopFlushThread();
    }
    std::cout << "The Server receive Crtl+C, will been closed\n";
    longjmp(jmpbuf, 1);
}

void *client_handler(void *sock_fd) {
    int count = 0;
    int fd = *((int *)sock_fd);
    int i_recvBytes;
    char data_recv[BUFFER_LENGTH];
    // const char *data_send = "Server has received your request!\n";
    char *data_send = new char[BUFFER_LENGTH];
    int offset = 0;
    // the latest transaction's txn_id
    txn_id_t txn_id = INVALID_TXN_ID;
    //    const std::string end = "\n";

    while (true) {
        std::cout << "Waiting for request..." << std::endl;
        memset(data_recv, 0, BUFFER_LENGTH);

        i_recvBytes = read(fd, data_recv, BUFFER_LENGTH);

        if (i_recvBytes == 0) {
            std::cout << "Maybe the client has closed" << std::endl;
            break;
        }
        if (i_recvBytes == -1) {
            std::cout << "Client read error!" << std::endl;
            break;
        }
        if (strcmp(data_recv, "exit") == 0) {
            std::cout << "Client exit." << std::endl;
            break;
        }
        count++;
        std::cout << "Read from client " << fd << ": " << data_recv << std::endl;

        memset(data_send, 0, BUFFER_LENGTH);
        offset = 0;

        // TODO: call function named "exec_simple_sql"
        //        RC rc = sql_handler.exec_simple_sql(data_recv);
        add_history(data_recv);
        YY_BUFFER_STATE buf = yy_scan_string(data_recv);
        if (yyparse() == 0) {
            if (ast::parse_tree != nullptr) {
                Context *context = new Context(lock_manager.get(), log_manager.get(), nullptr, data_send, &offset);
                try {
                    interp->interp_sql(ast::parse_tree, &txn_id, context);
                    // memcpy(data_send + offset, end, strlen(end) + 1);
                    // offset += strlen(end) + 1;
                } catch (TransactionAbortException &e) {
                    std::string str = e.GetInfo();
                    memcpy(data_send, str.c_str(), str.length());
                    data_send[str.length()] = '\0';
                    txn_manager->Abort(context->txn_, log_manager.get());
                } catch (RedBaseError &e) {
                    std::cerr << e.what() << std::endl;
                }
            }
        }
        yy_delete_buffer(buf);
        // TODO: 格式化 sql_handler.result, 传给客户端
        // send result with fixed format, use protobuf in the future
        if (write(fd, data_send, offset + 1) == -1) {
            break;
        }
    }

    // Clear
    std::cout << "Terminating current client_connection..." << std::endl;
    close(fd);           // close a file descriptor.
    pthread_exit(NULL);  // terminate calling thread!
}

void start_server() {
    int sockfd_server;
    int sockfd;
    int fd_temp;
    struct sockaddr_in s_addr_in {};
    struct sockaddr_in s_addr_client {};
    int client_length;

    sockfd_server = socket(AF_INET, SOCK_STREAM, 0);  // ipv4,TCP
    assert(sockfd_server != -1);
    int val = 1;
    setsockopt(sockfd_server, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    // before bind(), set the attr of structure sockaddr.
    memset(&s_addr_in, 0, sizeof(s_addr_in));
    s_addr_in.sin_family = AF_INET;
    s_addr_in.sin_addr.s_addr = htonl(INADDR_ANY);
    s_addr_in.sin_port = htons(SOCK_PORT);
    fd_temp = bind(sockfd_server, (struct sockaddr *)(&s_addr_in), sizeof(s_addr_in));
    if (fd_temp == -1) {
        std::cout << "Bind error!" << std::endl;
        exit(1);
    }

    fd_temp = listen(sockfd_server, MAX_CONN_LIMIT);
    if (fd_temp == -1) {
        std::cout << "Listen error!" << std::endl;
        exit(1);
    }

    if (recovery->GetRecoveryMode()) {
        recovery->Redo();
        recovery->Undo();
    }

    if (log_manager->GetLogMode()) {
        log_manager->RunFlushThread();
    }

    while (!should_exit) {
        std::cout << "Waiting for new connection..." << std::endl;
        pthread_t thread_id;
        client_length = sizeof(s_addr_client);

        if (setjmp(jmpbuf)) {
            std::cout << "Break from Server Listen Loop\n";
            break;
        }

        // Block here. Until server accepts a new connection.
        sockfd = accept(sockfd_server, (struct sockaddr *)(&s_addr_client), (socklen_t *)(&client_length));
        if (sockfd == -1) {
            std::cout << "Accept error!" << std::endl;
            continue;  // ignore current socket ,continue while loop.
        }
        std::cout << "A new connection occurs!" << std::endl;
        if (pthread_create(&thread_id, nullptr, &client_handler, (void *)(&sockfd)) != 0) {
            std::cout << "Create thread fail!" << std::endl;
            break;  // break while loop
        }
    }

    // Clear
    std::cout << " Try to close all client-connection.\n";
    int ret = shutdown(sockfd_server, SHUT_WR);  // shut down the all or part of a full-duplex connection.
    if(ret == -1) { printf("%s\n", strerror(errno)); }
//    assert(ret != -1);
    sm_manager->close_db();
    std::cout << " DB has been closed.\n";
    std::cout << "Server shuts down." << std::endl;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <database>" << std::endl;
        exit(1);
    }

    signal(SIGINT, sigint_handler);
    try {
        std::cout << "\n"
                     "   ____  _   _  ____ ____    _    ____  _____ \n"
                     "  |  _ \\| | | |/ ___| __ )  / \\  / ___|| ____|\n"
                     "  | |_) | | | | |   |  _ \\ / _ \\ \\___ \\|  _|  \n"
                     "  |  _ <| |_| | |___| |_) / ___ \\ ___) | |___ \n"
                     "  |_| \\_ \\___/ \\____|____/_/   \\_\\____/|_____|\n"
                     "\n"
                     "Welcome to RUC Database !\n"
                     "Type 'help;' for help.\n"
                     "\n";
        // Database name is passed by args
        std::string db_name = argv[1];
        if (!sm_manager->is_dir(db_name)) {
            // Database not found, create a new one
            sm_manager->create_db(db_name);
        }
        // Open database
        sm_manager->open_db(db_name);

        start_server();
    } catch (RedBaseError &e) {
        std::cerr << e.what() << std::endl;
        exit(1);
    }
    return 0;
}
