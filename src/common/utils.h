#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stddef.h>

// 时间戳函数
long long get_timestamp();

// 目录操作函数
int create_directory(const char* path);

// 文件操作函数
int file_exists(const char* filepath);
long get_file_size(const char* filepath);

// 字符串操作函数
void generate_random_string(char* buffer, size_t length);
void trim_whitespace(char* str);

// 校验和函数
uint32_t calculate_checksum(const void* data, size_t length);

#endif // UTILS_H