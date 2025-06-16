#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>

// 初始化服务器
int server_init(int port) {
    // 初始化服务器状态
    memset(&g_server, 0, sizeof(server_state_t));
    g_server.port = port;
    g_server.running = 0;
    g_server.client_count = 0;
    
    // 初始化互斥锁
    if (pthread_mutex_init(&g_server.clients_mutex, NULL) != 0) {
        perror("pthread_mutex_init clients_mutex");
        return -1;
    }
    
    if (pthread_mutex_init(&g_server.db_mutex, NULL) != 0) {
        perror("pthread_mutex_init db_mutex");
        pthread_mutex_destroy(&g_server.clients_mutex);
        return -1;
    }
    
    // 创建socket
    g_server.server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server.server_socket == -1) {
        perror("socket");
        pthread_mutex_destroy(&g_server.clients_mutex);
        pthread_mutex_destroy(&g_server.db_mutex);
        return -1;
    }
    
    // 设置socket选项
    int opt = 1;
    if (setsockopt(g_server.server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        close(g_server.server_socket);
        pthread_mutex_destroy(&g_server.clients_mutex);
        pthread_mutex_destroy(&g_server.db_mutex);
        return -1;
    }
    
    // 绑定地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(g_server.server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(g_server.server_socket);
        pthread_mutex_destroy(&g_server.clients_mutex);
        pthread_mutex_destroy(&g_server.db_mutex);
        return -1;
    }
    
    // 开始监听
    if (listen(g_server.server_socket, MAX_CLIENTS) == -1) {
        perror("listen");
        close(g_server.server_socket);
        pthread_mutex_destroy(&g_server.clients_mutex);
        pthread_mutex_destroy(&g_server.db_mutex);
        return -1;
    }
    
    printf("服务器socket初始化成功，监听端口 %d\n", port);
    return 0;
}

// 清理服务器资源
void server_cleanup() {
    g_server.running = 0;
    
    // 关闭所有客户端连接
    pthread_mutex_lock(&g_server.clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_server.clients[i].active) {
            disconnect_client(&g_server.clients[i]);
        }
    }
    pthread_mutex_unlock(&g_server.clients_mutex);
    
    // 关闭服务器socket
    if (g_server.server_socket > 0) {
        close(g_server.server_socket);
        g_server.server_socket = 0;
    }
    
    // 销毁互斥锁
    pthread_mutex_destroy(&g_server.clients_mutex);
    pthread_mutex_destroy(&g_server.db_mutex);
}

// 启动服务器
int server_start() {
    g_server.running = 1;
    printf("服务器开始接受连接...\n");
    
    while (g_server.running) {
        if (accept_client_connection() == -1) {
            if (g_server.running) {
                fprintf(stderr, "接受客户端连接失败\n");
            }
            break;
        }
    }
    
    return 0;
}

// 停止服务器
void server_stop() {
    g_server.running = 0;
    
    // 关闭服务器socket以中断accept调用
    if (g_server.server_socket > 0) {
        shutdown(g_server.server_socket, SHUT_RDWR);
    }
}

// 接受客户端连接
int accept_client_connection() {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    
    int client_socket = accept(g_server.server_socket, 
                              (struct sockaddr*)&client_addr, 
                              &client_addr_len);
    
    if (client_socket == -1) {
        if (errno != EINTR && g_server.running) {
            perror("accept");
        }
        return -1;
    }
    
    // 查找空闲的客户端槽位
    pthread_mutex_lock(&g_server.clients_mutex);
    
    int client_index = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!g_server.clients[i].active) {
            client_index = i;
            break;
        }
    }
    
    if (client_index == -1) {
        pthread_mutex_unlock(&g_server.clients_mutex);
        printf("服务器已达到最大客户端连接数，拒绝新连接\n");
        close(client_socket);
        return -1;
    }
    
    // 初始化客户端连接
    client_connection_t* client = &g_server.clients[client_index];
    memset(client, 0, sizeof(client_connection_t));
    client->socket_fd = client_socket;
    client->address = client_addr;
    client->active = 1;
    client->connect_time = time(NULL);
    client->last_heartbeat = client->connect_time;
    strcpy(client->client_version, "unknown");
    
    // 创建客户端处理线程
    if (pthread_create(&client->thread_id, NULL, client_handler, client) != 0) {
        perror("pthread_create");
        client->active = 0;
        close(client_socket);
        pthread_mutex_unlock(&g_server.clients_mutex);
        return -1;
    }
    
    // 分离线程，让其自动清理
    pthread_detach(client->thread_id);
    
    g_server.client_count++;
    pthread_mutex_unlock(&g_server.clients_mutex);
    
    printf("新客户端连接: %s:%d (索引: %d)\n", 
           inet_ntoa(client_addr.sin_addr), 
           ntohs(client_addr.sin_port),
           client_index);
    
    return 0;
}

// 断开客户端连接
void disconnect_client(client_connection_t* client) {
    if (!client || !client->active) {
        return;
    }
    
    printf("断开客户端连接: %s:%d\n", 
           inet_ntoa(client->address.sin_addr), 
           ntohs(client->address.sin_port));
    
    client->active = 0;
    
    if (client->socket_fd > 0) {
        close(client->socket_fd);
        client->socket_fd = 0;
    }
    
    g_server.client_count--;
}

// 记录客户端连接日志
void log_client_connection(client_connection_t* client, const char* action) {
    if (!client) return;
    
    printf("[%s] 客户端 %s:%d (版本: %s)\n", 
           action,
           inet_ntoa(client->address.sin_addr),
           ntohs(client->address.sin_port),
           client->client_version);
}

// 客户端处理线程
void* client_handler(void* arg) {
    client_connection_t* client = (client_connection_t*)arg;
    char buffer[BUFFER_SIZE];
    
    log_client_connection(client, "连接");
    
    while (client->active && g_server.running) {
        // 接收消息头
        message_header_t header;
        ssize_t received = recv(client->socket_fd, &header, sizeof(header), MSG_WAITALL);
        
        if (received <= 0) {
            if (received == 0) {
                printf("客户端正常断开连接\n");
            } else {
                perror("recv header");
            }
            break;
        }
        
        if (received != sizeof(header)) {
            printf("接收到不完整的消息头\n");
            break;
        }
        
        // 验证消息头
        if (!validate_message_header(&header)) {
            printf("无效的消息头\n");
            send_error_response(client, "无效的消息头");
            break;
        }
        
        // 接收消息数据
        char* data = NULL;
        if (header.length > 0) {
            data = malloc(header.length + 1);
            if (!data) {
                printf("内存分配失败\n");
                send_error_response(client, "服务器内存不足");
                break;
            }
            
            received = recv(client->socket_fd, data, header.length, MSG_WAITALL);
            if (received != (ssize_t)header.length) {
                printf("接收消息数据失败\n");
                free(data);
                break;
            }
            data[header.length] = '\0';
        }
        
        // 验证校验和
        uint32_t calculated_checksum = calculate_checksum(data, header.length);
        if (calculated_checksum != header.checksum) {
            printf("消息校验和不匹配\n");
            send_error_response(client, "数据校验失败");
            free(data);
            break;
        }
        
        // 处理消息
        if (handle_client_message(client, &header, data) != 0) {
            printf("处理客户端消息失败\n");
            free(data);
            break;
        }
        
        free(data);
        
        // 更新心跳时间
        client->last_heartbeat = time(NULL);
    }
    
    log_client_connection(client, "断开");
    
    // 清理客户端连接
    pthread_mutex_lock(&g_server.clients_mutex);
    disconnect_client(client);
    pthread_mutex_unlock(&g_server.clients_mutex);
    
    return NULL;
}