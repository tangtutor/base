#ifndef CLIENT_H
#define CLIENT_H

#include "../common/protocol.h"
#include "../common/base64.h"
#include <pthread.h>
#include <gtk/gtk.h>

// 客户端配置
#define CLIENT_VERSION "1.0.0"
#define CONFIG_FILE "client.conf"
#define UPDATE_DIR "updates/"
#define TEMP_DIR "temp/"
#define HEARTBEAT_INTERVAL 60  // 心跳间隔（秒）

// 默认配置值
#define DEFAULT_SERVER_HOST "localhost"
#define DEFAULT_SERVER_PORT 8888
#define DEFAULT_HEARTBEAT_INTERVAL 60
#define DEFAULT_CONNECTION_TIMEOUT 30
#define DEFAULT_MAX_RETRY 3

// 连接状态
typedef enum {
    CONN_DISCONNECTED = 0,
    CONN_CONNECTING,
    CONN_CONNECTED,
    CONN_ERROR
} connection_status_t;

// 客户端配置结构
typedef struct {
    char server_host[256];
    int server_port;
    int auto_update;
    int heartbeat_enabled;
    char download_dir[512];
    int log_level;
    int heartbeat_interval;
    int connection_timeout;
    int max_retry_count;
} client_config_t;

// 客户端状态结构
typedef struct {
    int socket_fd;
    connection_status_t status;
    pthread_t network_thread;
    pthread_t heartbeat_thread;
    pthread_mutex_t status_mutex;
    pthread_mutex_t send_mutex;
    int running;
    int gui_mode;
    client_config_t config;
    char server_version[32];
    char latest_version[32];
    int update_available;
} client_state_t;

// GUI相关结构
typedef struct {
    GtkWidget* main_window;
    GtkWidget* status_label;
    GtkWidget* server_entry;
    GtkWidget* port_entry;
    GtkWidget* connect_button;
    GtkWidget* disconnect_button;
    GtkWidget* file_chooser_button;
    GtkWidget* upload_button;
    GtkWidget* progress_bar;
    GtkWidget* log_textview;
    GtkTextBuffer* log_buffer;
    GtkWidget* table_entry;
    GtkWidget* field_entry;
    GtkWidget* data_entry;
    GtkWidget* data_upload_button;
} gui_widgets_t;

// 全局客户端状态
extern client_state_t g_client;
extern gui_widgets_t g_gui;

// 网络相关函数
int client_init();
void client_cleanup();
int client_connect(const char* host, int port);
void client_disconnect();
int client_send_message(message_type_t type, const void* data, size_t data_size);
int client_receive_message(message_header_t* header, char** data);
void* network_thread_func(void* arg);
void* heartbeat_thread_func(void* arg);

// 消息处理函数
int send_version_check();
int send_update_request();
int send_file_upload(const char* filename);
int send_data_upload(const char* table_name, const char* field_name, const char* data);
int send_heartbeat();

// 响应处理函数
int handle_version_response(version_response_msg_t* response);
int handle_update_data(const char* data, size_t data_size);
void handle_file_response(const char* data, size_t data_len);
void handle_data_response(const char* data, size_t data_len);
void handle_error_response(const char* data, size_t data_len);

// 更新相关函数
int check_for_updates();
int download_update(const char* data, size_t data_size);
int apply_update();
int restart_client();

// 文件处理函数
int upload_file(const char* filename);
char* read_file_content(const char* filename, size_t* file_size);
int create_directories();

// 配置相关函数
int load_config();
int save_config();
void set_default_config();

// GUI相关函数
int gui_init(int argc, char* argv[]);
void gui_cleanup();
void gui_run();
void gui_update_status(const char* status);
void gui_log_message(const char* format, ...);
void gui_set_progress(double progress);
void gui_show_error(const char* message);
void gui_show_info(const char* message);

// GUI回调函数
void on_connect_clicked(GtkWidget* widget, gpointer data);
void on_disconnect_clicked(GtkWidget* widget, gpointer data);
void on_file_upload_clicked(GtkWidget* widget, gpointer data);
void on_data_upload_clicked(GtkWidget* widget, gpointer data);
void on_window_destroy(GtkWidget* widget, gpointer data);
void on_file_chosen(GtkFileChooserButton* widget, gpointer data);

// 工具函数
void log_message_to_gui(const char* format, ...);
const char* get_connection_status_string(connection_status_t status);
int is_connected();
void set_connection_status(connection_status_t status);
connection_status_t get_connection_status();

// 线程安全的状态管理
void lock_status();
void unlock_status();
void lock_send();
void unlock_send();

#endif // CLIENT_H