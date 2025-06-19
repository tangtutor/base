#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// 初始化数据库
int database_init() {
    // 创建数据库目录
    struct stat st = {0};
    if (stat("data/database", &st) == -1) {
        if (mkdir("data/database", 0755) == -1) {
            perror("mkdir data/database");
            return -1;
        }
    }
    
    // 打开数据库
    int rc = sqlite3_open(DATABASE_PATH, &g_server.database);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "无法打开数据库: %s\n", sqlite3_errmsg(g_server.database));
        sqlite3_close(g_server.database);
        g_server.database = NULL;
        return -1;
    }
    
    printf("数据库连接成功: %s\n", DATABASE_PATH);
    
    // 创建表
    if (database_create_tables() != 0) {
        fprintf(stderr, "创建数据库表失败\n");
        database_cleanup();
        return -1;
    }
    
    return 0;
}

// 清理数据库资源
void database_cleanup() {
    if (g_server.database) {
        sqlite3_close(g_server.database);
        g_server.database = NULL;
        printf("数据库连接已关闭\n");
    }
}

// 创建数据库表
int database_create_tables() {
    const char* sql_create_tables[] = {
        // 客户端信息表
        "CREATE TABLE IF NOT EXISTS clients ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "client_ip TEXT NOT NULL,"
        "client_version TEXT,"
        "connect_time DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "disconnect_time DATETIME,"
        "status TEXT DEFAULT 'connected'"
        ");",
        
        // 文件上传记录表
        "CREATE TABLE IF NOT EXISTS file_uploads ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "client_ip TEXT NOT NULL,"
        "filename TEXT NOT NULL,"
        "file_size INTEGER,"
        "upload_time DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "file_path TEXT,"
        "status TEXT DEFAULT 'completed'"
        ");",
        
        // 数据字段表（动态表，用于存储各种字段数据）
        "CREATE TABLE IF NOT EXISTS field_data ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "client_ip TEXT NOT NULL,"
        "table_name TEXT NOT NULL,"
        "field_name TEXT NOT NULL,"
        "data_value BLOB,"
        "data_size INTEGER,"
        "upload_time DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");",
        
        // 系统日志表
        "CREATE TABLE IF NOT EXISTS system_logs ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "log_level TEXT NOT NULL,"
        "message TEXT NOT NULL,"
        "client_ip TEXT,"
        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");",
        
        // 版本信息表
        "CREATE TABLE IF NOT EXISTS version_info ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "version TEXT NOT NULL UNIQUE,"
        "release_date DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "update_file_path TEXT,"
        "description TEXT,"
        "is_latest INTEGER DEFAULT 0"
        ");"
    };
    
    pthread_mutex_lock(&g_server.db_mutex);
    
    for (size_t i = 0; i < sizeof(sql_create_tables) / sizeof(sql_create_tables[0]); i++) {
        char* err_msg = NULL;
        int rc = sqlite3_exec(g_server.database, sql_create_tables[i], NULL, NULL, &err_msg);
        
        if (rc != SQLITE_OK) {
            fprintf(stderr, "创建表失败: %s\n", err_msg);
            sqlite3_free(err_msg);
            pthread_mutex_unlock(&g_server.db_mutex);
            return -1;
        }
    }
    
    // 插入默认版本信息
    const char* sql_insert_version = 
        "INSERT OR IGNORE INTO version_info (version, description, is_latest) "
        "VALUES (?, '初始版本', 1)";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(g_server.database, sql_insert_version, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, SERVER_VERSION, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
    pthread_mutex_unlock(&g_server.db_mutex);
    
    printf("数据库表创建完成\n");
    return 0;
}

// 存储字段数据
int database_store_field_data(const char* table_name, const char* field_name, 
                             const unsigned char* data, size_t data_size) {
    if (!g_server.database || !table_name || !field_name || !data) {
        return -1;
    }
    
    const char* sql = 
        "INSERT INTO field_data (client_ip, table_name, field_name, data_value, data_size) "
        "VALUES (?, ?, ?, ?, ?)";
    
    pthread_mutex_lock(&g_server.db_mutex);
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(g_server.database, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "准备SQL语句失败: %s\n", sqlite3_errmsg(g_server.database));
        pthread_mutex_unlock(&g_server.db_mutex);
        return -1;
    }
    
    // 绑定参数
    sqlite3_bind_text(stmt, 1, "unknown", -1, SQLITE_STATIC); // TODO: 获取实际客户端IP
    sqlite3_bind_text(stmt, 2, table_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, field_name, -1, SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 4, data, data_size, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, (int)data_size);
    
    rc = sqlite3_step(stmt);
    
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "执行SQL语句失败: %s\n", sqlite3_errmsg(g_server.database));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&g_server.db_mutex);
        return -1;
    }
    
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_server.db_mutex);
    
    printf("字段数据已存储: 表=%s, 字段=%s, 大小=%zu字节\n", 
           table_name, field_name, data_size);
    
    return 0;
}

// 记录客户端连接
int database_log_client_connection(const char* client_ip, const char* client_version, 
                                  const char* action) {
    if (!g_server.database || !client_ip || !action) {
        return -1;
    }
    
    const char* sql;
    if (strcmp(action, "connect") == 0) {
        sql = "INSERT INTO clients (client_ip, client_version, status) VALUES (?, ?, 'connected')";
    } else {
        sql = "UPDATE clients SET disconnect_time = CURRENT_TIMESTAMP, status = 'disconnected' "
              "WHERE client_ip = ? AND status = 'connected'";
    }
    
    pthread_mutex_lock(&g_server.db_mutex);
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(g_server.database, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "准备SQL语句失败: %s\n", sqlite3_errmsg(g_server.database));
        pthread_mutex_unlock(&g_server.db_mutex);
        return -1;
    }
    
    sqlite3_bind_text(stmt, 1, client_ip, -1, SQLITE_STATIC);
    if (strcmp(action, "connect") == 0 && client_version) {
        sqlite3_bind_text(stmt, 2, client_version, -1, SQLITE_STATIC);
    }
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    pthread_mutex_unlock(&g_server.db_mutex);
    
    return (rc == SQLITE_DONE) ? 0 : -1;
}

// 记录文件上传
int database_log_file_upload(const char* client_ip, const char* filename, 
                            size_t file_size, const char* file_path) {
    if (!g_server.database || !client_ip || !filename || !file_path) {
        return -1;
    }
    
    const char* sql = 
        "INSERT INTO file_uploads (client_ip, filename, file_size, file_path) "
        "VALUES (?, ?, ?, ?)";
    
    pthread_mutex_lock(&g_server.db_mutex);
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(g_server.database, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "准备SQL语句失败: %s\n", sqlite3_errmsg(g_server.database));
        pthread_mutex_unlock(&g_server.db_mutex);
        return -1;
    }
    
    sqlite3_bind_text(stmt, 1, client_ip, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, filename, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)file_size);
    sqlite3_bind_text(stmt, 4, file_path, -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    pthread_mutex_unlock(&g_server.db_mutex);
    
    return (rc == SQLITE_DONE) ? 0 : -1;
}

// 记录系统日志
int database_log_system_event(const char* level, const char* message, const char* client_ip) {
    if (!g_server.database || !level || !message) {
        return -1;
    }
    
    const char* sql = 
        "INSERT INTO system_logs (log_level, message, client_ip) "
        "VALUES (?, ?, ?)";
    
    pthread_mutex_lock(&g_server.db_mutex);
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(g_server.database, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "准备SQL语句失败: %s\n", sqlite3_errmsg(g_server.database));
        pthread_mutex_unlock(&g_server.db_mutex);
        return -1;
    }
    
    sqlite3_bind_text(stmt, 1, level, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, message, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, client_ip, -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    pthread_mutex_unlock(&g_server.db_mutex);
    
    return (rc == SQLITE_DONE) ? 0 : -1;
}

// 获取最新版本信息
int database_get_latest_version(char* version_buffer, size_t buffer_size) {
    if (!g_server.database || !version_buffer) {
        return -1;
    }
    
    const char* sql = "SELECT version FROM version_info WHERE is_latest = 1 LIMIT 1";
    
    pthread_mutex_lock(&g_server.db_mutex);
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(g_server.database, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "准备SQL语句失败: %s\n", sqlite3_errmsg(g_server.database));
        pthread_mutex_unlock(&g_server.db_mutex);
        return -1;
    }
    
    rc = sqlite3_step(stmt);
    
    if (rc == SQLITE_ROW) {
        const char* version = (const char*)sqlite3_column_text(stmt, 0);
        if (version) {
            strncpy(version_buffer, version, buffer_size - 1);
            version_buffer[buffer_size - 1] = '\0';
        }
    } else {
        // 如果没有找到，使用服务器版本
        strncpy(version_buffer, SERVER_VERSION, buffer_size - 1);
        version_buffer[buffer_size - 1] = '\0';
    }
    
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_server.db_mutex);
    
    return 0;
}

// 更新服务器版本到数据库
int database_update_server_version(const char* new_version) {
    if (!g_server.database || !new_version) {
        return -1;
    }
    
    pthread_mutex_lock(&g_server.db_mutex);
    
    // 将所有现有版本设置为非最新
    const char* sql_update_old = "UPDATE version_info SET is_latest = 0";
    char* err_msg = NULL;
    int rc = sqlite3_exec(g_server.database, sql_update_old, NULL, NULL, &err_msg);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "更新旧版本状态失败: %s\n", err_msg);
        sqlite3_free(err_msg);
        pthread_mutex_unlock(&g_server.db_mutex);
        return -1;
    }
    
    // 插入新版本
    const char* sql_insert = 
        "INSERT OR REPLACE INTO version_info (version, description, is_latest) "
        "VALUES (?, '服务器版本更新', 1)";
    
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(g_server.database, sql_insert, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "准备插入版本SQL失败: %s\n", sqlite3_errmsg(g_server.database));
        pthread_mutex_unlock(&g_server.db_mutex);
        return -1;
    }
    
    sqlite3_bind_text(stmt, 1, new_version, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    pthread_mutex_unlock(&g_server.db_mutex);
    
    if (rc == SQLITE_DONE) {
        printf("服务器版本已更新到数据库: %s\n", new_version);
        return 0;
    } else {
        fprintf(stderr, "插入新版本失败\n");
        return -1;
    }
}