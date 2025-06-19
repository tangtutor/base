#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

// 发送版本检查
int send_version_check() {
    if (!is_connected()) {
        return -1;
    }
    
    version_check_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    
    // 设置客户端版本
    strncpy(msg.client_version, CLIENT_VERSION, sizeof(msg.client_version) - 1);
    
    // 设置平台信息
    #ifdef __linux__
        strncpy(msg.platform, "Linux", sizeof(msg.platform) - 1);
    #elif defined(__APPLE__)
        strncpy(msg.platform, "macOS", sizeof(msg.platform) - 1);
    #elif defined(_WIN32)
        strncpy(msg.platform, "Windows", sizeof(msg.platform) - 1);
    #else
        strncpy(msg.platform, "Unknown", sizeof(msg.platform) - 1);
    #endif
    
    printf("发送版本检查: %s (%s)\n", msg.client_version, msg.platform);
    
    return client_send_message(MSG_VERSION_CHECK, &msg, sizeof(msg));
}

// 发送更新请求
int send_update_request() {
    if (!is_connected()) {
        return -1;
    }
    
    printf("请求更新文件...\n");
    return client_send_message(MSG_UPDATE_REQUEST, NULL, 0);
}

// 发送心跳
int send_heartbeat() {
    if (!is_connected()) {
        return -1;
    }
    
    return client_send_message(MSG_HEARTBEAT, NULL, 0);
}

// 处理版本响应
int handle_version_response(version_response_msg_t* response) {
    if (!response) {
        return -1;
    }
    
    printf("收到版本响应: 状态=%d\n", response->status);
    
    // 更新服务器版本信息
    strncpy(g_client.server_version, response->server_version, sizeof(g_client.server_version) - 1);
    strncpy(g_client.latest_version, response->latest_version, sizeof(g_client.latest_version) - 1);
    
    switch (response->status) {
        case STATUS_SUCCESS:
            printf("版本检查成功\n");
            printf("服务器版本: %s\n", response->server_version);
            break;
            
        case STATUS_UPDATE_AVAILABLE:
            printf("发现新版本: %s (当前版本: %s)\n", 
                   response->latest_version, CLIENT_VERSION);
            printf("更新包大小: %u 字节\n", response->update_size);
            
            g_client.update_available = 1;
            
            // 如果启用了自动更新，立即请求更新
            if (g_client.config.auto_update) {
                printf("自动更新已启用，开始下载更新...\n");
                send_update_request();
            } else {
                printf("有新版本可用，请手动更新\n");
                log_message_to_gui("发现新版本 %s，请更新", response->latest_version);
            }
            break;
            
        case STATUS_NO_UPDATE:
            printf("当前版本已是最新版本\n");
            g_client.update_available = 0;
            break;
            
        case STATUS_ERROR:
        default:
            printf("版本检查失败\n");
            break;
    }
    
    return 0;
}

// 处理更新数据
int handle_update_data(const char* data, size_t data_size) {
    if (!data || data_size == 0) {
        printf("收到空的更新数据\n");
        return -1;
    }
    
    printf("收到更新数据: %zu 字节\n", data_size);
    
    // 下载并应用更新
    if (download_update(data, data_size) == 0) {
        printf("更新下载成功\n");
        log_message_to_gui("更新下载完成，准备应用更新");
        
        // 应用更新
        if (apply_update() == 0) {
            printf("更新应用成功，准备重启...\n");
            log_message_to_gui("更新应用成功，程序将重启");
            
            // 延迟重启，给用户时间看到消息
            sleep(3);
            restart_client();
        } else {
            printf("更新应用失败\n");
            log_message_to_gui("更新应用失败");
        }
    } else {
        printf("更新下载失败\n");
        log_message_to_gui("更新下载失败");
    }
    
    return 0;
}

// 检查更新
int check_for_updates() {
    if (!is_connected()) {
        printf("未连接到服务器\n");
        return -1;
    }
    
    printf("检查更新...\n");
    return send_version_check();
}

// 下载更新
int download_update(const char* data, size_t data_size) {
    if (!data || data_size == 0) {
        return -1;
    }
    
    // 创建更新目录
    struct stat st = {0};
    if (stat(UPDATE_DIR, &st) == -1) {
        if (mkdir(UPDATE_DIR, 0755) == -1) {
            perror("mkdir update directory");
            return -1;
        }
    }
    
    // 解码Base64数据
    size_t decoded_size = base64_decoded_length(data, data_size);
    unsigned char* decoded_data = malloc(decoded_size);
    
    if (!decoded_data) {
        fprintf(stderr, "内存分配失败\n");
        return -1;
    }
    
    int decode_result = base64_decode(data, data_size, decoded_data, decoded_size);
    if (decode_result == -1) {
        fprintf(stderr, "Base64解码失败\n");
        free(decoded_data);
        return -1;
    }
    
    // 保存更新文件
    char update_file_path[512];
    snprintf(update_file_path, sizeof(update_file_path), "%sclient_update.tar.gz", UPDATE_DIR);
    
    FILE* file = fopen(update_file_path, "wb");
    if (!file) {
        fprintf(stderr, "无法创建更新文件: %s\n", strerror(errno));
        free(decoded_data);
        return -1;
    }
    
    size_t written = fwrite(decoded_data, 1, decode_result, file);
    fclose(file);
    free(decoded_data);
    
    if (written != (size_t)decode_result) {
        fprintf(stderr, "更新文件写入不完整\n");
        remove(update_file_path);
        return -1;
    }
    
    printf("更新文件已保存: %s (%zu 字节)\n", update_file_path, written);
    return 0;
}

// 应用更新
int apply_update() {
    char update_file_path[512];
    char extract_dir[512];
    char command[1024];
    
    snprintf(update_file_path, sizeof(update_file_path), "%sclient_update.tar.gz", UPDATE_DIR);
    snprintf(extract_dir, sizeof(extract_dir), "%sextracted/", UPDATE_DIR);
    
    // 检查更新文件是否存在
    if (access(update_file_path, F_OK) != 0) {
        printf("更新文件不存在: %s\n", update_file_path);
        return -1;
    }
    
    // 创建解压目录
    struct stat st = {0};
    if (stat(extract_dir, &st) == -1) {
        if (mkdir(extract_dir, 0755) == -1) {
            perror("mkdir extract directory");
            return -1;
        }
    }
    
    // 解压更新文件
    snprintf(command, sizeof(command), "tar -xzf %s -C %s", update_file_path, extract_dir);
    
    printf("解压更新文件: %s\n", command);
    int result = system(command);
    
    if (result != 0) {
        printf("解压更新文件失败\n");
        return -1;
    }
    
    // 检查新的可执行文件
    char new_client_path[512];
    snprintf(new_client_path, sizeof(new_client_path), "%sclient", extract_dir);
    
    if (access(new_client_path, X_OK) != 0) {
        printf("新的客户端可执行文件不存在或无执行权限: %s\n", new_client_path);
        return -1;
    }
    
    // 备份当前可执行文件
    char backup_path[512];
    snprintf(backup_path, sizeof(backup_path), "%sclient.backup", UPDATE_DIR);
    
    char current_exe[512];
    ssize_t len = readlink("/proc/self/exe", current_exe, sizeof(current_exe) - 1);
    if (len != -1) {
        current_exe[len] = '\0';
        
        snprintf(command, sizeof(command), "cp %s %s", current_exe, backup_path);
        if (system(command) != 0) {
            printf("备份当前可执行文件失败\n");
            return -1;
        }
        
        // 替换可执行文件
        snprintf(command, sizeof(command), "cp %s %s", new_client_path, current_exe);
        if (system(command) != 0) {
            printf("替换可执行文件失败\n");
            // 尝试恢复备份
            snprintf(command, sizeof(command), "cp %s %s", backup_path, current_exe);
            system(command);
            return -1;
        }
        
        printf("更新应用成功\n");
        return 0;
    } else {
        printf("无法获取当前可执行文件路径\n");
        return -1;
    }
}

// 重启客户端
int restart_client() {
    printf("重启客户端...\n");
    
    // 保存配置
    save_config();
    
    // 获取当前可执行文件路径
    char current_exe[512];
    ssize_t len = readlink("/proc/self/exe", current_exe, sizeof(current_exe) - 1);
    if (len != -1) {
        current_exe[len] = '\0';
        
        // 断开连接
        client_disconnect();
        
        // 执行新程序
        execl(current_exe, current_exe, NULL);
        
        // 如果execl失败，程序会继续执行到这里
        perror("execl");
        return -1;
    } else {
        printf("无法获取当前可执行文件路径\n");
        return -1;
    }
}

// 创建必要的目录
int create_directories() {
    const char* dirs[] = {
        UPDATE_DIR,
        TEMP_DIR,
        "downloads/"
    };
    
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        struct stat st = {0};
        if (stat(dirs[i], &st) == -1) {
            if (mkdir(dirs[i], 0755) == -1) {
                perror("mkdir");
                printf("创建目录失败: %s\n", dirs[i]);
                return -1;
            }
            printf("创建目录: %s\n", dirs[i]);
        }
    }
    
    return 0;
}