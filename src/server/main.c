#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

// 全局服务器状态
server_state_t g_server = {0};

// 信号处理函数
void signal_handler(int signal) {
    switch (signal) {
        case SIGINT:
        case SIGTERM:
            printf("\n收到终止信号，正在关闭服务器...\n");
            server_stop();
            break;
        case SIGPIPE:
            // 忽略SIGPIPE信号，避免客户端断开连接时程序崩溃
            break;
    }
}

// 设置信号处理
void setup_signal_handlers() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);
}

// 打印使用说明
void print_usage(const char* program_name) {
    printf("使用方法: %s [选项]\n", program_name);
    printf("选项:\n");
    printf("  -p <端口>    指定服务器端口 (默认: %d)\n", DEFAULT_PORT);
    printf("  -h           显示此帮助信息\n");
    printf("  -v           显示版本信息\n");
}

// 打印版本信息
void print_version() {
    printf("Base64网络传输服务器 版本 %s\n", SERVER_VERSION);
    printf("协议版本: %s\n", PROTOCOL_VERSION);
}

// 打印服务器状态
void print_server_status() {
    printf("\n=== 服务器状态 ===\n");
    printf("版本: %s\n", SERVER_VERSION);
    printf("端口: %d\n", g_server.port);
    printf("运行状态: %s\n", g_server.running ? "运行中" : "已停止");
    printf("连接的客户端数量: %d\n", get_client_count());
    printf("数据库状态: %s\n", g_server.database ? "已连接" : "未连接");
    printf("==================\n\n");
}

// 获取当前客户端数量
int get_client_count() {
    int count = 0;
    pthread_mutex_lock(&g_server.clients_mutex);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_server.clients[i].active) {
            count++;
        }
    }
    
    pthread_mutex_unlock(&g_server.clients_mutex);
    return count;
}

// 清理非活跃客户端
void cleanup_inactive_clients() {
    time_t current_time = time(NULL);
    
    pthread_mutex_lock(&g_server.clients_mutex);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_server.clients[i].active) {
            // 检查心跳超时（5分钟）
            if (current_time - g_server.clients[i].last_heartbeat > 300) {
                printf("客户端 %d 心跳超时，断开连接\n", i);
                disconnect_client(&g_server.clients[i]);
            }
        }
    }
    
    pthread_mutex_unlock(&g_server.clients_mutex);
}

// 状态监控线程
void* status_monitor_thread(void* arg) {
    (void)arg; // 避免未使用参数警告
    
    while (g_server.running) {
        sleep(60); // 每分钟检查一次
        cleanup_inactive_clients();
        
        // 每10分钟打印一次状态
        static int counter = 0;
        if (++counter >= 10) {
            print_server_status();
            counter = 0;
        }
    }
    
    return NULL;
}

int main(int argc, char* argv[]) {
    int port = DEFAULT_PORT;
    int opt;
    
    // 解析命令行参数
    while ((opt = getopt(argc, argv, "p:hv")) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                if (port <= 0 || port > 65535) {
                    fprintf(stderr, "错误: 无效的端口号 %d\n", port);
                    return 1;
                }
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'v':
                print_version();
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    printf("启动Base64网络传输服务器...\n");
    print_version();
    
    // 设置信号处理
    setup_signal_handlers();
    
    // 初始化服务器
    if (server_init(port) != 0) {
        fprintf(stderr, "服务器初始化失败\n");
        return 1;
    }
    
    // 初始化数据库
    if (database_init() != 0) {
        fprintf(stderr, "数据库初始化失败\n");
        server_cleanup();
        return 1;
    }
    
    // 创建上传目录
    if (create_upload_directory() != 0) {
        fprintf(stderr, "创建上传目录失败\n");
        database_cleanup();
        server_cleanup();
        return 1;
    }
    
    printf("服务器初始化完成，监听端口 %d\n", port);
    print_server_status();
    
    // 启动状态监控线程
    pthread_t monitor_thread;
    if (pthread_create(&monitor_thread, NULL, status_monitor_thread, NULL) != 0) {
        fprintf(stderr, "创建监控线程失败\n");
    }
    
    // 启动服务器主循环
    int result = server_start();
    
    // 等待监控线程结束
    if (monitor_thread) {
        pthread_cancel(monitor_thread);
        pthread_join(monitor_thread, NULL);
    }
    
    // 清理资源
    printf("正在清理资源...\n");
    database_cleanup();
    server_cleanup();
    
    printf("服务器已关闭\n");
    return result;
}