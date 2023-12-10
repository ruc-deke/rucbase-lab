#include <unistd.h>
#include <string>

#define MAX_MEM_BUFFER_SIZE 8192
#define PORT_DEFAULT 8765

int init_unix_sock(const char *unix_sock_path);
int init_tcp_sock(const char *server_host, int server_port);
int connect_database(const char* unix_sockect_path, const char* server_host, int server_port);
void disconnect(int sockfd);
int send_sql(int sockfd, const std::string& sql);
int send_recv_sql(int sockfd, const std::string& sql, char* recv_buf);
void start_test(int sockfd, std::string infile);