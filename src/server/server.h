#ifndef SERVER_H
#define SERVER_H

#include "../common/protocol.h"
#include "../common/base64.h"
#include <pthread.h>
#include <sqlite3.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <time.h>

// 服务器配置
#define MAX_CLIENTS 100
#define SERVER_VERSION "1.0.0"
#define UPDATE_FILE_PATH "data/updates/client_update.tar.gz"
#define DATABASE_PATH "data/database/server.db"
#define UPLOAD_DIR "data/uploads/"

// 客户端连接结构
typedef struct {
    int socket_fd;
    pthread_t thread_id;
    struct sockaddr_in address;
    int active;
    char client_version[32];
    time_t connect_time;
    time_t last_heartbeat;
} client_connection_t;

// 服务器状态结构
typedef struct {
    int server_socket;
    int running;
    int port;
    pthread_mutex_t clients_mutex;
    client_connection_t clients[MAX_CLIENTS];
    int client_count;
    sqlite3* database;
    pthread_mutex_t db_mutex;
} server_state_t;

// 文件传输状态
typedef struct {
    char filename[MAX_FILENAME_LEN];
    FILE* file_handle;
    uint32_t total_size;
    uint32_t received_size;
    time_t start_time;
} file_transfer_t;

// 全局服务器状态
extern server_state_t g_server;

// 网络相关函数
int server_init(int port);
void server_cleanup();
int server_start();
void server_stop();
void* client_handler(void* arg);
int accept_client_connection();
void disconnect_client(client_connection_t* client);

// 消息处理函数
int handle_client_message(client_connection_t* client, message_header_t* header, char* data);
int handle_version_check(client_connection_t* client, version_check_msg_t* msg);
int handle_update_request(client_connection_t* client);
int handle_file_upload(client_connection_t* client, file_upload_msg_t* msg);
int handle_data_upload(client_connection_t* client, data_upload_msg_t* msg);
int handle_heartbeat(client_connection_t* client);

// 响应发送函数
int send_version_response(client_connection_t* client, status_code_t status);
int send_file_response(client_connection_t* client, status_code_t status, const char* message);
int send_data_response(client_connection_t* client, status_code_t status, const char* message);
int send_error_response(client_connection_t* client, const char* error_message);

// 数据库相关函数
int database_init();
void database_cleanup();
int database_store_field_data(const char* table_name, const char* field_name, 
                             const unsigned char* data, size_t data_size);
int database_create_tables();

// 文件处理函数
int save_uploaded_file(const char* filename, const unsigned char* data, size_t data_size);
int create_upload_directory();
char* get_upload_file_path(const char* filename);

// 更新相关函数
int check_update_available(const char* client_version);
int send_update_file(client_connection_t* client);

// 工具函数
void log_client_connection(client_connection_t* client, const char* action);
void cleanup_inactive_clients();
int get_client_count();
void print_server_status();

// 信号处理
void setup_signal_handlers();
void signal_handler(int signal);

#endif // SERVER_H