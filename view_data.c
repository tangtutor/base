#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include "src/common/base64.h"

#define DATABASE_PATH "data/database/server.db"

int main() {
    sqlite3* db;
    sqlite3_stmt* stmt;
    int rc;
    
    // 打开数据库
    rc = sqlite3_open(DATABASE_PATH, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "无法打开数据库: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }
    
    printf("=== 上传数据查看工具 ===\n\n");
    
    // 查询所有上传的数据
    const char* sql = "SELECT id, client_ip, table_name, field_name, data_value, data_size, upload_time FROM field_data ORDER BY upload_time DESC";
    
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "准备SQL语句失败: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }
    
    int count = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        count++;
        int id = sqlite3_column_int(stmt, 0);
        const char* client_ip = (const char*)sqlite3_column_text(stmt, 1);
        const char* table_name = (const char*)sqlite3_column_text(stmt, 2);
        const char* field_name = (const char*)sqlite3_column_text(stmt, 3);
        const void* data_blob = sqlite3_column_blob(stmt, 4);
        int data_size = sqlite3_column_int(stmt, 5);
        const char* upload_time = (const char*)sqlite3_column_text(stmt, 6);
        
        printf("=== 记录 %d ===\n", count);
        printf("ID: %d\n", id);
        printf("客户端IP: %s\n", client_ip ? client_ip : "N/A");
        printf("表名: %s\n", table_name ? table_name : "N/A");
        printf("字段名: %s\n", field_name ? field_name : "N/A");
        printf("上传时间: %s\n", upload_time ? upload_time : "N/A");
        printf("数据大小: %d 字节\n", data_size);
        
        if (data_blob && data_size > 0) {
            // 显示Base64编码的原始数据
            char* base64_str = malloc(data_size + 1);
            if (base64_str) {
                memcpy(base64_str, data_blob, data_size);
                base64_str[data_size] = '\0';
                printf("Base64编码: %s\n", base64_str);
                
                // Base64解码
                unsigned char decoded_buffer[512];
                int decoded_len = base64_decode(base64_str, strlen(base64_str), 
                                              decoded_buffer, sizeof(decoded_buffer));
                
                if (decoded_len > 0) {
                    decoded_buffer[decoded_len] = '\0';
                    printf("解码后数据: %s\n", decoded_buffer);
                    printf("解码后长度: %d 字节\n", decoded_len);
                } else {
                    printf("解码失败\n");
                }
                
                free(base64_str);
            } else {
                printf("内存分配失败\n");
            }
        } else {
            printf("无数据\n");
        }
        
        printf("\n");
    }
    
    if (count == 0) {
        printf("数据库中没有上传的数据。\n");
    }
    
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "查询执行失败: %s\n", sqlite3_errmsg(db));
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    printf("=== 查看完成 ===\n");
    return 0;
}