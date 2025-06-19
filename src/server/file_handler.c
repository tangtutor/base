#include "server.h"
#include "../common/base64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h>

// 创建上传目录
int create_upload_directory() {
    struct stat st = {0};
    
    if (stat(UPLOAD_DIR, &st) == -1) {
        if (mkdir(UPLOAD_DIR, 0755) == -1) {
            perror("mkdir upload directory");
            return -1;
        }
        printf("创建上传目录: %s\n", UPLOAD_DIR);
    }
    
    return 0;
}

// 获取上传文件的完整路径
char* get_upload_file_path(const char* filename) {
    if (!filename) return NULL;
    
    size_t path_len = strlen(UPLOAD_DIR) + strlen(filename) + 1;
    char* full_path = malloc(path_len);
    
    if (!full_path) {
        return NULL;
    }
    
    snprintf(full_path, path_len, "%s%s", UPLOAD_DIR, filename);
    return full_path;
}

// 生成唯一文件名（避免文件名冲突）
char* generate_unique_filename(const char* original_filename) {
    if (!original_filename) return NULL;
    
    time_t now = time(NULL);
    struct tm* timeinfo = localtime(&now);
    
    // 提取文件扩展名
    const char* ext = strrchr(original_filename, '.');
    if (!ext) ext = "";
    
    // 提取文件名（不含扩展名）
    size_t name_len = ext ? (size_t)(ext - original_filename) : strlen(original_filename);
    char* base_name = malloc(name_len + 1);
    if (!base_name) return NULL;
    
    strncpy(base_name, original_filename, name_len);
    base_name[name_len] = '\0';
    
    // 生成带时间戳的文件名
    char* unique_filename = malloc(256);
    if (!unique_filename) {
        free(base_name);
        return NULL;
    }
    
    snprintf(unique_filename, 256, "%s_%04d%02d%02d_%02d%02d%02d%s",
             base_name,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec,
             ext);
    
    free(base_name);
    return unique_filename;
}

// 保存上传的文件
int save_uploaded_file(const char* filename, const unsigned char* data, size_t data_size) {
    if (!filename || !data || data_size == 0) {
        return -1;
    }
    
    // 生成唯一文件名
    char* unique_filename = generate_unique_filename(filename);
    if (!unique_filename) {
        fprintf(stderr, "生成唯一文件名失败\n");
        return -1;
    }
    
    // 获取完整路径
    char* full_path = get_upload_file_path(unique_filename);
    if (!full_path) {
        fprintf(stderr, "获取文件路径失败\n");
        free(unique_filename);
        return -1;
    }
    
    // 打开文件进行写入
    FILE* file = fopen(full_path, "wb");
    if (!file) {
        fprintf(stderr, "无法创建文件 %s: %s\n", full_path, strerror(errno));
        free(unique_filename);
        free(full_path);
        return -1;
    }
    
    // 写入数据
    size_t written = fwrite(data, 1, data_size, file);
    fclose(file);
    
    if (written != data_size) {
        fprintf(stderr, "文件写入不完整: 期望 %zu 字节，实际写入 %zu 字节\n", 
                data_size, written);
        remove(full_path); // 删除不完整的文件
        free(unique_filename);
        free(full_path);
        return -1;
    }
    
    printf("文件保存成功: %s (大小: %zu 字节)\n", full_path, data_size);
    
    // 记录到数据库
    database_log_file_upload("unknown", unique_filename, data_size, full_path);
    
    free(unique_filename);
    free(full_path);
    return 0;
}

// 处理文件上传消息
int handle_file_upload(client_connection_t* client, file_upload_msg_t* msg) {
    if (!client || !msg) {
        return -1;
    }
    
    printf("处理文件上传: %s (大小: %u 字节)\n", msg->filename, msg->file_size);
    
    // 解码Base64数据
    size_t decoded_size = base64_decoded_length(msg->data, msg->chunk_size);
    unsigned char* decoded_data = malloc(decoded_size);
    
    if (!decoded_data) {
        fprintf(stderr, "内存分配失败\n");
        send_file_response(client, STATUS_SERVER_ERROR, "服务器内存不足");
        return -1;
    }
    
    int decode_result = base64_decode(msg->data, msg->chunk_size, decoded_data, decoded_size);
    if (decode_result == -1) {
        fprintf(stderr, "Base64解码失败\n");
        free(decoded_data);
        send_file_response(client, STATUS_ERROR, "数据解码失败");
        return -1;
    }
    
    // 保存文件
    int save_result = save_uploaded_file(msg->filename, decoded_data, decode_result);
    free(decoded_data);
    
    // 获取客户端IP地址（避免inet_ntoa静态缓冲区问题）
    char client_ip[INET_ADDRSTRLEN];
    strncpy(client_ip, inet_ntoa(client->address.sin_addr), sizeof(client_ip) - 1);
    client_ip[sizeof(client_ip) - 1] = '\0';
    
    if (save_result == 0) {
        send_file_response(client, STATUS_SUCCESS, "文件上传成功");
        
        // 记录系统日志
        char log_msg[512];
        snprintf(log_msg, sizeof(log_msg), "文件上传成功: %s", msg->filename);
        database_log_system_event("INFO", log_msg, client_ip);
    } else {
        send_file_response(client, STATUS_SERVER_ERROR, "文件保存失败");
        
        // 记录错误日志
        char log_msg[512];
        snprintf(log_msg, sizeof(log_msg), "文件上传失败: %s", msg->filename);
        database_log_system_event("ERROR", log_msg, client_ip);
    }
    
    return save_result;
}

// 处理数据上传消息
int handle_data_upload(client_connection_t* client, data_upload_msg_t* msg) {
    if (!client || !msg) {
        return -1;
    }
    
    printf("处理数据上传: 表=%s, 字段=%s (大小: %u 字节)\n", 
           msg->table_name, msg->field_name, msg->data_size);
    
    // 解码Base64数据
    size_t decoded_size = base64_decoded_length(msg->data, msg->data_size);
    unsigned char* decoded_data = malloc(decoded_size);
    
    if (!decoded_data) {
        fprintf(stderr, "内存分配失败\n");
        send_data_response(client, STATUS_SERVER_ERROR, "服务器内存不足");
        return -1;
    }
    
    int decode_result = base64_decode(msg->data, msg->data_size, decoded_data, decoded_size);
    if (decode_result == -1) {
        fprintf(stderr, "Base64解码失败\n");
        free(decoded_data);
        send_data_response(client, STATUS_ERROR, "数据解码失败");
        return -1;
    }
    
    // 存储到数据库
    int store_result = database_store_field_data(msg->table_name, msg->field_name, 
                                               decoded_data, decode_result);
    free(decoded_data);
    
    // 获取客户端IP地址（避免inet_ntoa静态缓冲区问题）
    char client_ip[INET_ADDRSTRLEN];
    strncpy(client_ip, inet_ntoa(client->address.sin_addr), sizeof(client_ip) - 1);
    client_ip[sizeof(client_ip) - 1] = '\0';
    
    if (store_result == 0) {
        send_data_response(client, STATUS_SUCCESS, "数据上传成功");
        
        // 记录系统日志
        char log_msg[512];
        snprintf(log_msg, sizeof(log_msg), "数据上传成功: %s.%s", 
                msg->table_name, msg->field_name);
        database_log_system_event("INFO", log_msg, client_ip);
    } else {
        send_data_response(client, STATUS_SERVER_ERROR, "数据存储失败");
        
        // 记录错误日志
        char log_msg[512];
        snprintf(log_msg, sizeof(log_msg), "数据上传失败: %s.%s", 
                msg->table_name, msg->field_name);
        database_log_system_event("ERROR", log_msg, client_ip);
    }
    
    return store_result;
}

// 发送文件响应
int send_file_response(client_connection_t* client, status_code_t status, const char* message) {
    if (!client) return -1;
    
    response_msg_t response;
    memset(&response, 0, sizeof(response));
    response.status = status;
    
    if (message) {
        strncpy(response.message, message, sizeof(response.message) - 1);
    }
    
    // 创建消息头
    message_header_t header;
    init_message_header(&header, MSG_FILE_RESPONSE, sizeof(response));
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
    
    return 0;
}

// 发送数据响应
int send_data_response(client_connection_t* client, status_code_t status, const char* message) {
    if (!client) return -1;
    
    response_msg_t response;
    memset(&response, 0, sizeof(response));
    response.status = status;
    
    if (message) {
        strncpy(response.message, message, sizeof(response.message) - 1);
    }
    
    // 创建消息头
    message_header_t header;
    init_message_header(&header, MSG_DATA_RESPONSE, sizeof(response));
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
    
    return 0;
}

// 发送错误响应
int send_error_response(client_connection_t* client, const char* error_message) {
    if (!client) return -1;
    
    response_msg_t response;
    memset(&response, 0, sizeof(response));
    response.status = STATUS_ERROR;
    
    if (error_message) {
        strncpy(response.message, error_message, sizeof(response.message) - 1);
    }
    
    // 创建消息头
    message_header_t header;
    init_message_header(&header, MSG_ERROR, sizeof(response));
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
    
    return 0;
}