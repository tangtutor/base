#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <getopt.h>

// 全局客户端状态
client_state_t g_client = {0};
gui_widgets_t g_gui = {0};
int gui_mode_active = 0; // GUI模式标志

// 信号处理函数
void signal_handler(int signal) {
    switch (signal) {
        case SIGINT:
        case SIGTERM:
            printf("\n收到终止信号，正在关闭客户端...\n");
            g_client.running = 0;
            client_disconnect();
            gtk_main_quit();
            break;
        case SIGPIPE:
            // 忽略SIGPIPE信号
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
    printf("使用方法: %s [选项|命令]\n", program_name);
    printf("选项:\n");
    printf("  -s <服务器>  指定服务器地址 (默认: localhost)\n");
    printf("  -p <端口>    指定服务器端口 (默认: %d)\n", DEFAULT_PORT);
    printf("  -h           显示此帮助信息\n");
    printf("  -v           显示版本信息\n");
    printf("  --gui        启动图形界面模式 (默认)\n");
    printf("  --no-gui     以命令行模式运行\n");
    printf("\n命令:\n");
    printf("  update       检查并下载更新\n");
}

// 打印版本信息
void print_version() {
    printf("Base64网络传输客户端 版本 %s\n", CLIENT_VERSION);
    printf("协议版本: %s\n", PROTOCOL_VERSION);
}

// 命令行模式主循环
void command_line_mode() {
    char input[1024];
    char command[64];
    char arg1[512], arg2[512], arg3[512];
    
    printf("\n=== Base64网络传输客户端 ===\n");
    printf("输入 'help' 查看可用命令\n\n");
    
    while (g_client.running) {
        printf("> ");
        fflush(stdout);
        
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }
        
        // 解析命令
        int argc = sscanf(input, "%63s %511s %511s %511s", command, arg1, arg2, arg3);
        
        if (argc < 1) continue;
        
        if (strcmp(command, "help") == 0) {
            printf("可用命令:\n");
            printf("  connect <服务器> <端口>  - 连接到服务器\n");
            printf("  disconnect               - 断开连接\n");
            printf("  upload <文件路径>        - 上传文件\n");
            printf("  data <表名> <字段名> <数据> - 上传数据\n");
            printf("  status                   - 显示连接状态\n");
            printf("  update                   - 检查更新\n");
            printf("  quit                     - 退出程序\n");
        }
        else if (strcmp(command, "connect") == 0) {
            if (argc >= 3) {
                int port = atoi(arg2);
                if (port > 0 && port <= 65535) {
                    printf("正在连接到 %s:%d...\n", arg1, port);
                    if (client_connect(arg1, port) == 0) {
                        printf("连接成功\n");
                    } else {
                        printf("连接失败\n");
                    }
                } else {
                    printf("无效的端口号: %s\n", arg2);
                }
            } else {
                printf("用法: connect <服务器> <端口>\n");
            }
        }
        else if (strcmp(command, "disconnect") == 0) {
            client_disconnect();
            printf("已断开连接\n");
        }
        else if (strcmp(command, "upload") == 0) {
            if (argc >= 2) {
                if (is_connected()) {
                    printf("正在上传文件: %s\n", arg1);
                    if (upload_file(arg1) == 0) {
                        printf("文件上传成功\n");
                    } else {
                        printf("文件上传失败\n");
                    }
                } else {
                    printf("请先连接到服务器\n");
                }
            } else {
                printf("用法: upload <文件路径>\n");
            }
        }
        else if (strcmp(command, "data") == 0) {
            if (argc >= 4) {
                if (is_connected()) {
                    printf("正在上传数据: %s.%s\n", arg1, arg2);
                    if (send_data_upload(arg1, arg2, arg3) == 0) {
                        printf("数据上传成功\n");
                    } else {
                        printf("数据上传失败\n");
                    }
                } else {
                    printf("请先连接到服务器\n");
                }
            } else {
                printf("用法: data <表名> <字段名> <数据>\n");
            }
        }
        else if (strcmp(command, "status") == 0) {
            printf("连接状态: %s\n", get_connection_status_string(get_connection_status()));
            if (is_connected()) {
                printf("服务器版本: %s\n", g_client.server_version);
                if (g_client.update_available) {
                    printf("有新版本可用: %s\n", g_client.latest_version);
                }
            }
        }
        else if (strcmp(command, "update") == 0) {
            if (is_connected()) {
                printf("正在检查更新...\n");
                check_for_updates();
            } else {
                printf("请先连接到服务器\n");
            }
        }
        else if (strcmp(command, "quit") == 0 || strcmp(command, "exit") == 0) {
            break;
        }
        else {
            printf("未知命令: %s\n", command);
            printf("输入 'help' 查看可用命令\n");
        }
    }
}

int main(int argc, char* argv[]) {
    int gui_mode = 0; // 默认命令行模式
    int update_mode = 0;
    char* server_host = NULL;
    int server_port = 0;
    
    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "-v") == 0) {
            print_version();
            return 0;
        }
        else if (strcmp(argv[i], "--gui") == 0) {
            gui_mode = 1;
        }
        else if (strcmp(argv[i], "--no-gui") == 0) {
            gui_mode = 0;
        }
        else if (strcmp(argv[i], "update") == 0) {
            update_mode = 1;
            gui_mode = 0; // 更新模式强制使用命令行
        }
        else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            server_host = argv[++i];
        }
        else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            server_port = atoi(argv[++i]);
        }
        // 配置文件参数已移除
    }
    
    printf("启动Base64网络传输客户端...\n");
    print_version();
    
    // 设置信号处理
    setup_signal_handlers();
    
    // 初始化客户端
    if (client_init() != 0) {
        fprintf(stderr, "客户端初始化失败\n");
        return 1;
    }
    
    // 加载配置
    if (load_config() != 0) {
        printf("使用默认配置\n");
        set_default_config();
    }
    
    // 覆盖命令行参数
    if (server_host) {
        strncpy(g_client.config.server_host, server_host, sizeof(g_client.config.server_host) - 1);
    }
    if (server_port > 0) {
        g_client.config.server_port = server_port;
    }
    
    // 创建必要的目录
    create_directories();
    
    printf("客户端初始化完成\n");
    
    int result = 0;
    
    if (gui_mode) {
        // GUI模式
        printf("启动图形界面...\n");
        gui_mode_active = 1; // 设置GUI模式标志
        if (gui_init(argc, argv) == 0) {
            gui_run();
        } else {
            fprintf(stderr, "GUI初始化失败，切换到命令行模式\n");
            gui_mode = 0;
            gui_mode_active = 0;
        }
    }
    
    if (!gui_mode) {
        if (update_mode) {
            // 更新模式
            printf("正在检查更新...\n");
            
            // 连接到服务器
            if (client_connect(g_client.config.server_host, g_client.config.server_port) == 0) {
                printf("已连接到服务器 %s:%d\n", g_client.config.server_host, g_client.config.server_port);
                
                // 检查更新
                if (check_for_updates() == 0) {
                    if (g_client.update_available) {
                        printf("发现新版本: %s\n", g_client.latest_version);
                        printf("正在请求更新...\n");
                        
                        // 发送更新请求，服务器会通过handle_update_data回调处理
                        if (send_update_request() == 0) {
                            printf("更新请求已发送\n");
                            // 等待服务器响应和数据传输
                            sleep(2);
                        } else {
                            printf("更新请求失败\n");
                            result = 1;
                        }
                    } else {
                        printf("当前版本已是最新版本\n");
                    }
                } else {
                    printf("检查更新失败\n");
                    result = 1;
                }
                
                client_disconnect();
            } else {
                printf("无法连接到服务器 %s:%d\n", g_client.config.server_host, g_client.config.server_port);
                result = 1;
            }
        } else {
            // 命令行模式
            printf("运行在命令行模式\n");
            command_line_mode();
        }
    }
    
    // 清理资源
    printf("正在清理资源...\n");
    client_cleanup();
    
    if (gui_mode) {
        gui_cleanup();
    }
    
    printf("客户端已关闭\n");
    return result;
}