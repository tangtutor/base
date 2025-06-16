#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

// 默认配置文件路径
#define CONFIG_FILE "client.conf"

// 函数声明
void trim_string(char* str);

// 加载配置
int load_config(client_config_t* config) {
    if (!config) {
        return -1;
    }
    
    // 设置默认值
    strncpy(config->server_host, DEFAULT_SERVER_HOST, sizeof(config->server_host) - 1);
    config->server_host[sizeof(config->server_host) - 1] = '\0';
    config->server_port = DEFAULT_SERVER_PORT;
    config->auto_update = 1;
    config->heartbeat_interval = DEFAULT_HEARTBEAT_INTERVAL;
    config->connection_timeout = DEFAULT_CONNECTION_TIMEOUT;
    config->max_retry_count = DEFAULT_MAX_RETRY;
    config->log_level = 1; // INFO级别
    
    // 尝试从配置文件加载
    FILE* file = fopen(CONFIG_FILE, "r");
    if (!file) {
        printf("配置文件不存在，使用默认配置\n");
        return save_config(config); // 保存默认配置
    }
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // 跳过注释和空行
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }
        
        // 解析配置项
        char key[64], value[192];
        if (sscanf(line, "%63[^=]=%191s", key, value) == 2) {
            // 去除key和value的空白字符
            trim_string(key);
            trim_string(value);
            
            if (strcmp(key, "server_host") == 0) {
                strncpy(config->server_host, value, sizeof(config->server_host) - 1);
                config->server_host[sizeof(config->server_host) - 1] = '\0';
            } else if (strcmp(key, "server_port") == 0) {
                config->server_port = atoi(value);
                if (config->server_port <= 0 || config->server_port > 65535) {
                    config->server_port = DEFAULT_SERVER_PORT;
                }
            } else if (strcmp(key, "auto_update") == 0) {
                config->auto_update = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) ? 1 : 0;
            } else if (strcmp(key, "heartbeat_interval") == 0) {
                config->heartbeat_interval = atoi(value);
                if (config->heartbeat_interval < 5) {
                    config->heartbeat_interval = DEFAULT_HEARTBEAT_INTERVAL;
                }
            } else if (strcmp(key, "connection_timeout") == 0) {
                config->connection_timeout = atoi(value);
                if (config->connection_timeout < 5) {
                    config->connection_timeout = DEFAULT_CONNECTION_TIMEOUT;
                }
            } else if (strcmp(key, "max_retry_count") == 0) {
                config->max_retry_count = atoi(value);
                if (config->max_retry_count < 0) {
                    config->max_retry_count = DEFAULT_MAX_RETRY;
                }
            } else if (strcmp(key, "log_level") == 0) {
                config->log_level = atoi(value);
                if (config->log_level < 0 || config->log_level > 3) {
                    config->log_level = 1;
                }
            }
        }
    }
    
    fclose(file);
    printf("配置加载完成\n");
    return 0;
}

// 保存配置
int save_config(const client_config_t* config) {
    if (!config) {
        return -1;
    }
    
    FILE* file = fopen(CONFIG_FILE, "w");
    if (!file) {
        printf("错误: 无法保存配置文件: %s\n", strerror(errno));
        return -1;
    }
    
    fprintf(file, "# Base64网络传输客户端配置文件\n");
    fprintf(file, "# 此文件由程序自动生成和维护\n\n");
    
    fprintf(file, "# 服务器配置\n");
    fprintf(file, "server_host=%s\n", config->server_host);
    fprintf(file, "server_port=%d\n", config->server_port);
    fprintf(file, "\n");
    
    fprintf(file, "# 客户端行为配置\n");
    fprintf(file, "auto_update=%s\n", config->auto_update ? "true" : "false");
    fprintf(file, "heartbeat_interval=%d\n", config->heartbeat_interval);
    fprintf(file, "connection_timeout=%d\n", config->connection_timeout);
    fprintf(file, "max_retry_count=%d\n", config->max_retry_count);
    fprintf(file, "\n");
    
    fprintf(file, "# 日志级别 (0=DEBUG, 1=INFO, 2=WARN, 3=ERROR)\n");
    fprintf(file, "log_level=%d\n", config->log_level);
    
    fclose(file);
    printf("配置保存完成\n");
    return 0;
}

// 显示当前配置
void print_config(const client_config_t* config) {
    if (!config) {
        return;
    }
    
    printf("\n=== 客户端配置 ===\n");
    printf("服务器地址: %s\n", config->server_host);
    printf("服务器端口: %d\n", config->server_port);
    printf("自动更新: %s\n", config->auto_update ? "启用" : "禁用");
    printf("心跳间隔: %d 秒\n", config->heartbeat_interval);
    printf("连接超时: %d 秒\n", config->connection_timeout);
    printf("最大重试次数: %d\n", config->max_retry_count);
    printf("日志级别: %d\n", config->log_level);
    printf("==================\n\n");
}

// 验证配置
int validate_config(const client_config_t* config) {
    if (!config) {
        return -1;
    }
    
    // 检查服务器地址
    if (strlen(config->server_host) == 0) {
        printf("错误: 服务器地址不能为空\n");
        return -1;
    }
    
    // 检查端口范围
    if (config->server_port <= 0 || config->server_port > 65535) {
        printf("错误: 无效的端口号: %d\n", config->server_port);
        return -1;
    }
    
    // 检查心跳间隔
    if (config->heartbeat_interval < 5 || config->heartbeat_interval > 300) {
        printf("错误: 心跳间隔应在5-300秒之间\n");
        return -1;
    }
    
    // 检查连接超时
    if (config->connection_timeout < 5 || config->connection_timeout > 120) {
        printf("错误: 连接超时应在5-120秒之间\n");
        return -1;
    }
    
    // 检查重试次数
    if (config->max_retry_count < 0 || config->max_retry_count > 10) {
        printf("错误: 最大重试次数应在0-10之间\n");
        return -1;
    }
    
    return 0;
}

// 更新配置项
int update_config_item(client_config_t* config, const char* key, const char* value) {
    if (!config || !key || !value) {
        return -1;
    }
    
    if (strcmp(key, "server_host") == 0) {
        if (strlen(value) >= sizeof(config->server_host)) {
            printf("错误: 服务器地址太长\n");
            return -1;
        }
        strncpy(config->server_host, value, sizeof(config->server_host) - 1);
        config->server_host[sizeof(config->server_host) - 1] = '\0';
    } else if (strcmp(key, "server_port") == 0) {
        int port = atoi(value);
        if (port <= 0 || port > 65535) {
            printf("错误: 无效的端口号\n");
            return -1;
        }
        config->server_port = port;
    } else if (strcmp(key, "auto_update") == 0) {
        config->auto_update = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) ? 1 : 0;
    } else if (strcmp(key, "heartbeat_interval") == 0) {
        int interval = atoi(value);
        if (interval < 5 || interval > 300) {
            printf("错误: 心跳间隔应在5-300秒之间\n");
            return -1;
        }
        config->heartbeat_interval = interval;
    } else if (strcmp(key, "connection_timeout") == 0) {
        int timeout = atoi(value);
        if (timeout < 5 || timeout > 120) {
            printf("错误: 连接超时应在5-120秒之间\n");
            return -1;
        }
        config->connection_timeout = timeout;
    } else if (strcmp(key, "max_retry_count") == 0) {
        int retry = atoi(value);
        if (retry < 0 || retry > 10) {
            printf("错误: 最大重试次数应在0-10之间\n");
            return -1;
        }
        config->max_retry_count = retry;
    } else if (strcmp(key, "log_level") == 0) {
        int level = atoi(value);
        if (level < 0 || level > 3) {
            printf("错误: 日志级别应在0-3之间\n");
            return -1;
        }
        config->log_level = level;
    } else {
        printf("错误: 未知的配置项: %s\n", key);
        return -1;
    }
    
    return 0;
}

// 重置为默认配置
void reset_config_to_default(client_config_t* config) {
    if (!config) {
        return;
    }
    
    strncpy(config->server_host, DEFAULT_SERVER_HOST, sizeof(config->server_host) - 1);
    config->server_host[sizeof(config->server_host) - 1] = '\0';
    config->server_port = DEFAULT_SERVER_PORT;
    config->auto_update = 1;
    config->heartbeat_interval = DEFAULT_HEARTBEAT_INTERVAL;
    config->connection_timeout = DEFAULT_CONNECTION_TIMEOUT;
    config->max_retry_count = DEFAULT_MAX_RETRY;
    config->log_level = 1;
    
    printf("配置已重置为默认值\n");
}

// 从命令行参数更新配置
int update_config_from_args(client_config_t* config, int argc, char* argv[]) {
    if (!config) {
        return -1;
    }
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--host") == 0) {
            if (i + 1 < argc) {
                strncpy(config->server_host, argv[i + 1], sizeof(config->server_host) - 1);
                config->server_host[sizeof(config->server_host) - 1] = '\0';
                i++; // 跳过下一个参数
            }
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) {
                int port = atoi(argv[i + 1]);
                if (port > 0 && port <= 65535) {
                    config->server_port = port;
                }
                i++; // 跳过下一个参数
            }
        } else if (strcmp(argv[i], "--no-auto-update") == 0) {
            config->auto_update = 0;
        } else if (strcmp(argv[i], "--heartbeat") == 0) {
            if (i + 1 < argc) {
                int interval = atoi(argv[i + 1]);
                if (interval >= 5 && interval <= 300) {
                    config->heartbeat_interval = interval;
                }
                i++; // 跳过下一个参数
            }
        } else if (strcmp(argv[i], "--timeout") == 0) {
            if (i + 1 < argc) {
                int timeout = atoi(argv[i + 1]);
                if (timeout >= 5 && timeout <= 120) {
                    config->connection_timeout = timeout;
                }
                i++; // 跳过下一个参数
            }
        } else if (strcmp(argv[i], "--log-level") == 0) {
            if (i + 1 < argc) {
                int level = atoi(argv[i + 1]);
                if (level >= 0 && level <= 3) {
                    config->log_level = level;
                }
                i++; // 跳过下一个参数
            }
        }
    }
    
    return 0;
}

// 去除字符串首尾空白字符
void trim_string(char* str) {
    if (!str) return;
    
    // 去除尾部空白字符
    int len = strlen(str);
    while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\t' || 
                       str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[--len] = '\0';
    }
    
    // 去除头部空白字符
    char* start = str;
    while (*start == ' ' || *start == '\t') {
        start++;
    }
    
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

// 备份当前配置
int backup_config() {
    char backup_file[256];
    time_t now = time(NULL);
    struct tm* timeinfo = localtime(&now);
    
    strftime(backup_file, sizeof(backup_file), "client.conf.backup.%Y%m%d_%H%M%S", timeinfo);
    
    FILE* src = fopen(CONFIG_FILE, "r");
    if (!src) {
        return -1; // 原文件不存在
    }
    
    FILE* dst = fopen(backup_file, "w");
    if (!dst) {
        fclose(src);
        return -1;
    }
    
    char buffer[1024];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dst);
    }
    
    fclose(src);
    fclose(dst);
    
    printf("配置已备份到: %s\n", backup_file);
    return 0;
}

// 设置默认配置
void set_default_config() {
    // 这个函数可以用来初始化全局配置
    // 目前暂时为空实现
}