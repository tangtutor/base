#include "client.h"
#include "../common/utils.h"
#include "../common/base64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

// 上传文件
int upload_file(const char* filepath) {
    if (!filepath) {
        printf("错误: 文件路径为空\n");
        return -1;
    }
    
    if (!is_connected()) {
        printf("错误: 未连接到服务器\n");
        return -1;
    }
    
    // 检查文件是否存在
    if (access(filepath, F_OK) != 0) {
        printf("错误: 文件不存在: %s\n", filepath);
        return -1;
    }
    
    // 获取文件大小
    struct stat file_stat;
    if (stat(filepath, &file_stat) != 0) {
        printf("错误: 无法获取文件信息: %s\n", strerror(errno));
        return -1;
    }
    
    if (file_stat.st_size > MAX_MESSAGE_LEN) {
        printf("错误: 文件太大 (最大 %d 字节)\n", MAX_MESSAGE_LEN);
        return -1;
    }
    
    // 读取文件内容
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        printf("错误: 无法打开文件: %s\n", strerror(errno));
        return -1;
    }
    
    char* file_data = malloc(file_stat.st_size);
    if (!file_data) {
        printf("错误: 内存分配失败\n");
        fclose(file);
        return -1;
    }
    
    size_t bytes_read = fread(file_data, 1, file_stat.st_size, file);
    fclose(file);
    
    if (bytes_read != (size_t)file_stat.st_size) {
        printf("错误: 文件读取不完整\n");
        free(file_data);
        return -1;
    }
    
    free(file_data);
    
    // 发送文件上传消息
    int result = send_file_upload(filepath);
    
    if (result == 0) {
        printf("文件上传成功: %s\n", filepath);
    } else {
        printf("文件上传失败: %s\n", filepath);
    }
    
    return result;
}

// 发送文件上传消息
int send_file_upload(const char* filename) {
    if (!filename) {
        printf("错误: 无效的文件名\n");
        return -1;
    }
    
    if (!is_connected()) {
        printf("错误: 未连接到服务器\n");
        return -1;
    }
    
    // 读取文件内容
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("错误: 无法打开文件 %s\n", filename);
        return -1;
    }
    
    // 获取文件大小
    fseek(file, 0, SEEK_END);
    size_t data_len = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // 读取文件数据
    char* file_data = malloc(data_len);
    if (!file_data) {
        printf("错误: 内存分配失败\n");
        fclose(file);
        return -1;
    }

    if (fread(file_data, 1, data_len, file) != data_len) {
        printf("错误: 读取文件失败\n");
        free(file_data);
        fclose(file);
        return -1;
    }
    fclose(file);

    // Base64编码文件数据
    size_t encoded_size = base64_encoded_length(data_len);
    char* encoded_data = malloc(encoded_size + 1);
    if (!encoded_data) {
        printf("错误: 内存分配失败\n");
        free(file_data);
        return -1;
    }

    int encode_result = base64_encode((unsigned char*)file_data, data_len, encoded_data, encoded_size + 1);
    free(file_data);
    
    if (encode_result < 0) {
        printf("错误: Base64编码失败\n");
        free(encoded_data);
        return -1;
    }

    // 计算消息总大小（使用编码后的数据大小）
    size_t total_size = sizeof(file_upload_msg_t) + encode_result;

    if (total_size > MAX_MESSAGE_LEN) {
        printf("错误: 文件太大，无法发送\n");
        free(encoded_data);
        return -1;
    }

    // 分配消息缓冲区
    char* buffer = malloc(total_size);
    if (!buffer) {
        printf("错误: 内存分配失败\n");
        free(encoded_data);
        return -1;
    }

    // 构造文件上传消息
    file_upload_msg_t* msg = (file_upload_msg_t*)buffer;

    // 提取文件名（去掉路径）
    const char* basename = strrchr(filename, '/');
    if (basename) {
        basename++; // 跳过 '/'
    } else {
        basename = filename;
    }

    strncpy(msg->filename, basename, MAX_FILENAME_LEN - 1);
    msg->filename[MAX_FILENAME_LEN - 1] = '\0';
    msg->file_size = data_len;  // 原始文件大小
    msg->chunk_size = encode_result;  // 编码后的数据大小
    msg->chunk_offset = 0;

    // 复制编码后的数据
    memcpy(msg->data, encoded_data, encode_result);
    
    // 发送消息
    int result = client_send_message(MSG_FILE_UPLOAD, buffer, total_size);

    free(buffer);
    free(encoded_data);
    return result;
}

// 发送数据上传消息
int send_data_upload(const char* table_name, const char* field_name, const char* data) {
    if (!table_name || !field_name || !data) {
        printf("错误: 无效的数据上传参数\n");
        return -1;
    }
    
    if (!is_connected()) {
        printf("错误: 未连接到服务器\n");
        return -1;
    }
    
    // Base64编码数据
    size_t data_len = strlen(data);
    size_t encoded_len = base64_encoded_length(data_len);
    char* encoded_data = malloc(encoded_len + 1);
    if (!encoded_data) {
        printf("错误: 内存分配失败\n");
        return -1;
    }
    
    int encode_result = base64_encode((unsigned char*)data, data_len, encoded_data, encoded_len + 1);
    if (encode_result < 0) {
        printf("错误: Base64编码失败\n");
        free(encoded_data);
        return -1;
    }
    
    // 计算消息大小
    size_t encoded_data_len = strlen(encoded_data);
    size_t total_size = sizeof(data_upload_msg_t) + encoded_data_len;
    
    if (total_size > MAX_MESSAGE_LEN) {
        printf("错误: 数据太大，无法发送\n");
        free(encoded_data);
        return -1;
    }
    
    // 分配消息缓冲区
    char* buffer = malloc(total_size);
    if (!buffer) {
        printf("错误: 内存分配失败\n");
        free(encoded_data);
        return -1;
    }
    
    // 构造数据上传消息
    data_upload_msg_t* msg = (data_upload_msg_t*)buffer;
    strncpy(msg->table_name, table_name, sizeof(msg->table_name) - 1);
    strncpy(msg->field_name, field_name, sizeof(msg->field_name) - 1);
    msg->data_size = encoded_data_len;
    
    // 复制编码后的数据
    memcpy(msg->data, encoded_data, encoded_data_len);
    
    // 发送消息
    int result = client_send_message(MSG_DATA_UPLOAD, buffer, total_size);
    
    free(buffer);
    free(encoded_data);
    
    if (result == 0) {
        printf("数据上传成功: %s.%s\n", table_name, field_name);
    } else {
        printf("数据上传失败: %s.%s\n", table_name, field_name);
    }
    
    return result;
}

// 处理文件响应
void handle_file_response(const char* data, size_t data_len) {
    if (!data || data_len < sizeof(FileResponse)) {
        printf("错误: 无效的文件响应\n");
        return;
    }
    
    const FileResponse* response = (const FileResponse*)data;
    
    printf("文件上传响应: 状态=%d\n", response->status);
    
    if (response->message_len > 0 && data_len >= sizeof(FileResponse) + response->message_len) {
        const char* message = data + sizeof(FileResponse);
        char* msg_str = malloc(response->message_len + 1);
        if (msg_str) {
            memcpy(msg_str, message, response->message_len);
            msg_str[response->message_len] = '\0';
            printf("服务器消息: %s\n", msg_str);
            free(msg_str);
        }
    }
    
    // 在GUI模式下更新界面
    if (g_client.gui_mode) {
        if (response->status == STATUS_SUCCESS) {
            gui_log_message("文件上传成功");
            gui_set_progress(1.0);
        } else {
            gui_log_message("文件上传失败");
            gui_set_progress(0.0);
        }
    }
}

// 处理数据响应
void handle_data_response(const char* data, size_t data_len) {
    if (!data || data_len < sizeof(DataResponse)) {
        printf("错误: 无效的数据响应\n");
        return;
    }
    
    const DataResponse* response = (const DataResponse*)data;
    
    printf("数据上传响应: 状态=%d\n", response->status);
    
    if (response->data_len > 0 && data_len >= sizeof(DataResponse) + response->data_len) {
        const char* message = data + sizeof(DataResponse);
        char* msg_str = malloc(response->data_len + 1);
        if (msg_str) {
            memcpy(msg_str, message, response->data_len);
            msg_str[response->data_len] = '\0';
            printf("服务器消息: %s\n", msg_str);
            free(msg_str);
        }
    }
    
    // 在GUI模式下更新界面
    if (g_client.gui_mode) {
        if (response->status == STATUS_SUCCESS) {
            gui_log_message("数据上传成功");
        } else {
            gui_log_message("数据上传失败");
        }
    }
}

// 处理错误响应
void handle_error_response(const char* data, size_t data_len) {
    if (!data || data_len < sizeof(ErrorResponse)) {
        printf("错误: 无效的错误响应\n");
        return;
    }
    
    const ErrorResponse* response = (const ErrorResponse*)data;
    
    printf("服务器错误: 代码=%d\n", response->error_code);
    
    if (response->message_len > 0 && data_len >= sizeof(ErrorResponse) + response->message_len) {
        const char* message = data + sizeof(ErrorResponse);
        char* msg_str = malloc(response->message_len + 1);
        if (msg_str) {
            memcpy(msg_str, message, response->message_len);
            msg_str[response->message_len] = '\0';
            printf("错误消息: %s\n", msg_str);
            
            // 在GUI模式下显示错误
            if (g_client.gui_mode) {
                gui_show_error(msg_str);
                gui_log_message("服务器错误: %s", msg_str);
            }
            
            free(msg_str);
        }
    }
}

// 创建必要的目录
int create_client_directories() {
    const char* dirs[] = {
        "downloads",
        "temp",
        "update",
        "logs"
    };
    
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        if (create_directory(dirs[i]) != 0) {
            printf("警告: 无法创建目录 %s\n", dirs[i]);
        }
    }
    
    return 0;
}

// 保存下载的文件
int save_downloaded_file(const char* filename, const char* data, size_t data_len) {
    if (!filename || !data || data_len == 0) {
        printf("错误: 无效的文件保存参数\n");
        return -1;
    }
    
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "downloads/%s", filename);
    
    FILE* file = fopen(filepath, "wb");
    if (!file) {
        printf("错误: 无法创建文件 %s: %s\n", filepath, strerror(errno));
        return -1;
    }
    
    size_t bytes_written = fwrite(data, 1, data_len, file);
    fclose(file);
    
    if (bytes_written != data_len) {
        printf("错误: 文件写入不完整\n");
        unlink(filepath); // 删除不完整的文件
        return -1;
    }
    
    printf("文件保存成功: %s\n", filepath);
    return 0;
}

// 验证文件完整性
int verify_file_integrity(const char* filepath, uint32_t expected_checksum) {
    if (!filepath) {
        return -1;
    }
    
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        return -1;
    }
    
    // 计算文件校验和
    char buffer[4096];
    size_t bytes_read;
    uint32_t checksum = 0;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        checksum ^= calculate_checksum(buffer, bytes_read);
    }
    
    fclose(file);
    
    return (checksum == expected_checksum) ? 0 : -1;
}

// 清理临时文件
void cleanup_temp_files() {
    // 清理temp目录中的文件
    system("rm -f temp/*");
    printf("临时文件清理完成\n");
}