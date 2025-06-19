#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <arpa/inet.h>

// 处理客户端消息
int handle_client_message(client_connection_t* client, message_header_t* header, char* data) {
    if (!client || !header) {
        return -1;
    }
    
    switch (header->type) {
        case MSG_VERSION_CHECK: {
            version_check_msg_t* msg = (version_check_msg_t*)data;
            return handle_version_check(client, msg);
        }
        
        case MSG_UPDATE_REQUEST:
            return handle_update_request(client);
        
        case MSG_FILE_UPLOAD: {
            file_upload_msg_t* msg = (file_upload_msg_t*)data;
            return handle_file_upload(client, msg);
        }
        
        case MSG_DATA_UPLOAD: {
            data_upload_msg_t* msg = (data_upload_msg_t*)data;
            return handle_data_upload(client, msg);
        }
        
        case MSG_HEARTBEAT:
            return handle_heartbeat(client);
        
        default:
            printf("未知消息类型: %d\n", header->type);
            send_error_response(client, "未知消息类型");
            return -1;
    }
}

// 处理版本检查
int handle_version_check(client_connection_t* client, version_check_msg_t* msg) {
    if (!client || !msg) {
        return -1;
    }
    
    printf("版本检查: 客户端版本=%s, 平台=%s\n", msg->client_version, msg->platform);
    
    // 更新客户端信息
    strncpy(client->client_version, msg->client_version, sizeof(client->client_version) - 1);
    
    // 检查是否有更新可用
    int update_available = check_update_available(msg->client_version);
    
    status_code_t status = update_available ? STATUS_UPDATE_AVAILABLE : STATUS_NO_UPDATE;
    
    // 记录客户端连接 - 修复inet_ntoa静态缓冲区问题
    char client_ip[INET_ADDRSTRLEN];
    strncpy(client_ip, inet_ntoa(client->address.sin_addr), sizeof(client_ip) - 1);
    client_ip[sizeof(client_ip) - 1] = '\0';
    database_log_client_connection(client_ip, msg->client_version, "connect");
    
    return send_version_response(client, status);
}

// 处理更新请求
int handle_update_request(client_connection_t* client) {
    if (!client) {
        return -1;
    }
    
    printf("处理更新请求\n");
    
    // 检查更新文件是否存在
    struct stat st;
    if (stat(UPDATE_FILE_PATH, &st) == -1) {
        printf("更新文件不存在: %s\n", UPDATE_FILE_PATH);
        send_error_response(client, "更新文件不可用");
        return -1;
    }
    
    return send_update_file(client);
}

// 处理心跳消息
int handle_heartbeat(client_connection_t* client) {
    if (!client) {
        return -1;
    }
    
    // 更新心跳时间
    client->last_heartbeat = time(NULL);
    
    // 发送心跳响应
    message_header_t header;
    init_message_header(&header, MSG_HEARTBEAT, 0);
    header.checksum = 0;
    
    if (send(client->socket_fd, &header, sizeof(header), 0) != sizeof(header)) {
        perror("send heartbeat response");
        return -1;
    }
    
    return 0;
}

// 发送版本响应
int send_version_response(client_connection_t* client, status_code_t status) {
    if (!client) {
        return -1;
    }
    
    version_response_msg_t response;
    memset(&response, 0, sizeof(response));
    response.status = status;
    
    // 设置服务器版本
    strncpy(response.server_version, SERVER_VERSION, sizeof(response.server_version) - 1);
    
    // 获取最新版本
    database_get_latest_version(response.latest_version, sizeof(response.latest_version));
    
    // 如果有更新，设置更新包大小
    if (status == STATUS_UPDATE_AVAILABLE) {
        struct stat st;
        if (stat(UPDATE_FILE_PATH, &st) == 0) {
            response.update_size = (uint32_t)st.st_size;
        }
    }
    
    // 创建消息头
    message_header_t header;
    init_message_header(&header, MSG_VERSION_RESPONSE, sizeof(response));
    header.checksum = calculate_checksum(&response, sizeof(response));
    
    // 发送消息头
    if (send(client->socket_fd, &header, sizeof(header), 0) != sizeof(header)) {
        perror("send header");
        return -1;
    }
    
    // 发送响应数据
    if (send(client->socket_fd, &response, sizeof(response), 0) != sizeof(response)) {
        perror("send response");
        return -1;
    }
    
    printf("版本响应已发送: 状态=%d\n", status);
    return 0;
}

// 检查更新是否可用
int check_update_available(const char* client_version) {
    if (!client_version) {
        return 0;
    }
    
    char latest_version[32];
    if (database_get_latest_version(latest_version, sizeof(latest_version)) != 0) {
        return 0;
    }
    
    // 版本不匹配时都需要更新（支持升级和降级）
    int result = strcmp(client_version, latest_version);
    return result != 0; // 客户端版本与服务器版本不匹配时需要更新
}

// 发送更新文件
int send_update_file(client_connection_t* client) {
    if (!client) {
        return -1;
    }
    
    FILE* file = fopen(UPDATE_FILE_PATH, "rb");
    if (!file) {
        perror("fopen update file");
        send_error_response(client, "无法打开更新文件");
        return -1;
    }
    
    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        send_error_response(client, "更新文件为空");
        return -1;
    }
    
    // 读取文件内容
    unsigned char* file_data = malloc(file_size);
    if (!file_data) {
        fclose(file);
        send_error_response(client, "服务器内存不足");
        return -1;
    }
    
    size_t read_size = fread(file_data, 1, file_size, file);
    fclose(file);
    
    if (read_size != (size_t)file_size) {
        free(file_data);
        send_error_response(client, "读取更新文件失败");
        return -1;
    }
    
    // Base64编码
    size_t encoded_size = base64_encoded_length(file_size);
    char* encoded_data = malloc(encoded_size + 1);
    if (!encoded_data) {
        free(file_data);
        send_error_response(client, "服务器内存不足");
        return -1;
    }
    
    int encode_result = base64_encode(file_data, file_size, encoded_data, encoded_size + 1);
    free(file_data);
    
    if (encode_result == -1) {
        free(encoded_data);
        send_error_response(client, "文件编码失败");
        return -1;
    }
    
    // 发送更新数据
    message_header_t header;
    init_message_header(&header, MSG_UPDATE_DATA, encode_result);
    header.checksum = calculate_checksum(encoded_data, encode_result);
    
    // 发送消息头
    if (send(client->socket_fd, &header, sizeof(header), 0) != sizeof(header)) {
        perror("send header");
        free(encoded_data);
        return -1;
    }
    
    // 发送编码后的文件数据
    ssize_t sent = send(client->socket_fd, encoded_data, encode_result, 0);
    free(encoded_data);
    
    if (sent != encode_result) {
        perror("send update data");
        return -1;
    }
    
    printf("更新文件已发送: %ld 字节\n", file_size);
    
    // 记录系统日志
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "更新文件已发送给客户端，大小: %ld 字节", file_size);
    database_log_system_event("INFO", log_msg, inet_ntoa(client->address.sin_addr));
    
    return 0;
}