#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

// 初始化客户端
int client_init() {
    // 初始化客户端状态
    memset(&g_client, 0, sizeof(client_state_t));
    g_client.socket_fd = -1;
    g_client.status = CONN_DISCONNECTED;
    g_client.running = 1;
    g_client.update_available = 0;
    
    // 初始化互斥锁
    if (pthread_mutex_init(&g_client.status_mutex, NULL) != 0) {
        perror("pthread_mutex_init status_mutex");
        return -1;
    }
    
    if (pthread_mutex_init(&g_client.send_mutex, NULL) != 0) {
        perror("pthread_mutex_init send_mutex");
        pthread_mutex_destroy(&g_client.status_mutex);
        return -1;
    }
    
    printf("客户端初始化成功\n");
    return 0;
}

// 清理客户端资源
void client_cleanup() {
    g_client.running = 0;
    
    // 断开连接
    client_disconnect();
    
    // 等待线程结束
    if (g_client.network_thread) {
        pthread_cancel(g_client.network_thread);
        pthread_join(g_client.network_thread, NULL);
    }
    
    if (g_client.heartbeat_thread) {
        pthread_cancel(g_client.heartbeat_thread);
        pthread_join(g_client.heartbeat_thread, NULL);
    }
    
    // 销毁互斥锁
    pthread_mutex_destroy(&g_client.status_mutex);
    pthread_mutex_destroy(&g_client.send_mutex);
}

// 连接到服务器
int client_connect(const char* host, int port) {
    if (!host || port <= 0) {
        return -1;
    }
    
    // 如果已经连接，先断开
    if (is_connected()) {
        client_disconnect();
    }
    
    set_connection_status(CONN_CONNECTING);
    
    // 创建socket
    g_client.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_client.socket_fd == -1) {
        perror("socket");
        set_connection_status(CONN_ERROR);
        return -1;
    }
    
    // 解析主机名
    struct hostent* server = gethostbyname(host);
    if (!server) {
        fprintf(stderr, "无法解析主机名: %s\n", host);
        close(g_client.socket_fd);
        g_client.socket_fd = -1;
        set_connection_status(CONN_ERROR);
        return -1;
    }
    
    // 设置服务器地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    
    // 连接到服务器
    if (connect(g_client.socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        close(g_client.socket_fd);
        g_client.socket_fd = -1;
        set_connection_status(CONN_ERROR);
        return -1;
    }
    
    set_connection_status(CONN_CONNECTED);
    
    // 更新配置
    strncpy(g_client.config.server_host, host, sizeof(g_client.config.server_host) - 1);
    g_client.config.server_port = port;
    
    printf("已连接到服务器 %s:%d\n", host, port);
    
    // 启动网络处理线程
    if (pthread_create(&g_client.network_thread, NULL, network_thread_func, NULL) != 0) {
        perror("pthread_create network_thread");
        client_disconnect();
        return -1;
    }
    
    // 启动心跳线程
    if (g_client.config.heartbeat_enabled) {
        if (pthread_create(&g_client.heartbeat_thread, NULL, heartbeat_thread_func, NULL) != 0) {
            perror("pthread_create heartbeat_thread");
            // 心跳线程失败不影响主要功能
        }
    }
    
    // 发送版本检查
    send_version_check();
    
    return 0;
}

// 断开连接
void client_disconnect() {
    set_connection_status(CONN_DISCONNECTED);
    
    if (g_client.socket_fd > 0) {
        close(g_client.socket_fd);
        g_client.socket_fd = -1;
        printf("已断开服务器连接\n");
    }
}

// 发送消息
int client_send_message(message_type_t type, const void* data, size_t data_size) {
    if (!is_connected()) {
        return -1;
    }
    
    lock_send();
    
    // 创建消息头
    message_header_t header;
    init_message_header(&header, type, data_size);
    
    if (data && data_size > 0) {
        header.checksum = calculate_checksum(data, data_size);
    } else {
        header.checksum = 0;
    }
    
    // 发送消息头
    ssize_t sent = send(g_client.socket_fd, &header, sizeof(header), 0);
    if (sent != sizeof(header)) {
        perror("send header");
        unlock_send();
        client_disconnect();
        return -1;
    }
    
    // 发送消息数据
    if (data && data_size > 0) {
        sent = send(g_client.socket_fd, data, data_size, 0);
        if (sent != (ssize_t)data_size) {
            perror("send data");
            unlock_send();
            client_disconnect();
            return -1;
        }
    }
    
    unlock_send();
    return 0;
}

// 接收消息
int client_receive_message(message_header_t* header, char** data) {
    if (!header || !is_connected()) {
        return -1;
    }
    
    // 接收消息头
    ssize_t received = recv(g_client.socket_fd, header, sizeof(*header), MSG_WAITALL);
    if (received <= 0) {
        if (received == 0) {
            printf("服务器关闭了连接\n");
        } else {
            perror("recv header");
        }
        client_disconnect();
        return -1;
    }
    
    if (received != sizeof(*header)) {
        printf("接收到不完整的消息头\n");
        client_disconnect();
        return -1;
    }
    
    // 验证消息头
    if (!validate_message_header(header)) {
        printf("无效的消息头\n");
        client_disconnect();
        return -1;
    }
    
    // 接收消息数据
    *data = NULL;
    if (header->length > 0) {
        *data = malloc(header->length + 1);
        if (!*data) {
            printf("内存分配失败\n");
            client_disconnect();
            return -1;
        }
        
        received = recv(g_client.socket_fd, *data, header->length, MSG_WAITALL);
        if (received != (ssize_t)header->length) {
            printf("接收消息数据失败\n");
            free(*data);
            *data = NULL;
            client_disconnect();
            return -1;
        }
        
        (*data)[header->length] = '\0';
        
        // 验证校验和
        uint32_t calculated_checksum = calculate_checksum(*data, header->length);
        if (calculated_checksum != header->checksum) {
            printf("消息校验和不匹配\n");
            free(*data);
            *data = NULL;
            client_disconnect();
            return -1;
        }
    }
    
    return 0;
}

// 网络处理线程
void* network_thread_func(void* arg) {
    (void)arg; // 避免未使用参数警告
    
    while (g_client.running && is_connected()) {
        message_header_t header;
        char* data = NULL;
        
        if (client_receive_message(&header, &data) == 0) {
            // 处理接收到的消息
            switch (header.type) {
                case MSG_VERSION_RESPONSE: {
                    version_response_msg_t* response = (version_response_msg_t*)data;
                    handle_version_response(response);
                    break;
                }
                
                case MSG_UPDATE_DATA:
                    handle_update_data(data, header.length);
                    break;
                
                case MSG_FILE_RESPONSE: {
                    handle_file_response(data, header.length);
                    break;
                }
                
                case MSG_DATA_RESPONSE: {
                    handle_data_response(data, header.length);
                    break;
                }
                
                case MSG_ERROR: {
                    handle_error_response(data, header.length);
                    break;
                }
                
                case MSG_HEARTBEAT:
                    // 心跳响应，无需处理
                    break;
                
                default:
                    printf("收到未知消息类型: %d\n", header.type);
                    break;
            }
        }
        
        if (data) {
            free(data);
        }
    }
    
    return NULL;
}

// 心跳线程
void* heartbeat_thread_func(void* arg) {
    (void)arg; // 避免未使用参数警告
    
    while (g_client.running && is_connected()) {
        sleep(HEARTBEAT_INTERVAL);
        
        if (is_connected()) {
            send_heartbeat();
        }
    }
    
    return NULL;
}

// 线程安全的状态管理函数
void lock_status() {
    pthread_mutex_lock(&g_client.status_mutex);
}

void unlock_status() {
    pthread_mutex_unlock(&g_client.status_mutex);
}

void lock_send() {
    pthread_mutex_lock(&g_client.send_mutex);
}

void unlock_send() {
    pthread_mutex_unlock(&g_client.send_mutex);
}

void set_connection_status(connection_status_t status) {
    lock_status();
    g_client.status = status;
    unlock_status();
}

connection_status_t get_connection_status() {
    lock_status();
    connection_status_t status = g_client.status;
    unlock_status();
    return status;
}

int is_connected() {
    return get_connection_status() == CONN_CONNECTED;
}

const char* get_connection_status_string(connection_status_t status) {
    switch (status) {
        case CONN_DISCONNECTED: return "已断开";
        case CONN_CONNECTING: return "连接中";
        case CONN_CONNECTED: return "已连接";
        case CONN_ERROR: return "连接错误";
        default: return "未知状态";
    }
}