#include "utils.h"
#include "protocol.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>

// 计算简单校验和
uint32_t calculate_checksum(const void* data, size_t length) {
    const unsigned char* bytes = (const unsigned char*)data;
    uint32_t checksum = 0;
    
    for (size_t i = 0; i < length; i++) {
        checksum += bytes[i];
        checksum = (checksum << 1) | (checksum >> 31); // 循环左移
    }
    
    return checksum;
}

// 验证消息头
int validate_message_header(const message_header_t* header) {
    if (!header) return 0;
    
    // 检查魔数
    if (header->magic != PROTOCOL_MAGIC) {
        return 0;
    }
    
    // 检查消息类型
    if (header->type < MSG_VERSION_CHECK || header->type > MSG_DISCONNECT) {
        return 0;
    }
    
    // 检查数据长度（防止过大的数据包）
    if (header->length > 10 * 1024 * 1024) { // 10MB限制
        return 0;
    }
    
    return 1;
}

// 初始化消息头
void init_message_header(message_header_t* header, uint16_t type, uint32_t length) {
    if (!header) return;
    
    header->magic = PROTOCOL_MAGIC;
    header->version = 1;
    header->type = type;
    header->length = length;
    header->checksum = 0; // 将在发送前计算
}

// 获取当前时间戳
long long get_timestamp() {
    return (long long)time(NULL);
}

// 创建目录（如果不存在）
int create_directory(const char* path) {
    struct stat st = {0};
    
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0755) == -1) {
            perror("mkdir");
            return -1;
        }
    }
    
    return 0;
}

// 检查文件是否存在
int file_exists(const char* filename) {
    return access(filename, F_OK) == 0;
}

// 获取文件大小
long get_file_size(const char* filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

// 安全的字符串复制
int safe_strcpy(char* dest, const char* src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) {
        return -1;
    }
    
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
    
    return 0;
}

// 安全的字符串连接
int safe_strcat(char* dest, const char* src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) {
        return -1;
    }
    
    size_t dest_len = strlen(dest);
    if (dest_len >= dest_size - 1) {
        return -1; // 目标字符串已满
    }
    
    strncat(dest, src, dest_size - dest_len - 1);
    
    return 0;
}

// 从文件路径中提取文件名
const char* extract_filename(const char* filepath) {
    if (!filepath) return NULL;
    
    const char* filename = strrchr(filepath, '/');
    if (filename) {
        return filename + 1; // 跳过'/'
    }
    
    return filepath; // 没有路径分隔符，整个字符串就是文件名
}

// 生成随机字符串
void generate_random_string(char* buffer, size_t length) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    
    if (!buffer || length == 0) return;
    
    srand((unsigned int)time(NULL));
    
    for (size_t i = 0; i < length - 1; i++) {
        buffer[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    buffer[length - 1] = '\0';
}

// 日志记录函数
void log_message(const char* level, const char* format, ...) {
    time_t now;
    struct tm* timeinfo;
    char timestamp[64];
    
    time(&now);
    timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    printf("[%s] [%s] ", timestamp, level);
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    
    printf("\n");
    fflush(stdout);
}