#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

// 协议版本
#define PROTOCOL_VERSION "1.0"

// 默认端口
#define DEFAULT_PORT 8888

// 缓冲区大小
#define BUFFER_SIZE 4096
#define MAX_FILENAME_LEN 256
#define MAX_MESSAGE_LEN 1024

// 消息类型
typedef enum {
    MSG_VERSION_CHECK = 1,    // 版本检查
    MSG_VERSION_RESPONSE,     // 版本响应
    MSG_UPDATE_REQUEST,       // 更新请求
    MSG_UPDATE_DATA,          // 更新数据
    MSG_FILE_UPLOAD,          // 文件上传
    MSG_FILE_RESPONSE,        // 文件响应
    MSG_DATA_UPLOAD,          // 数据上传
    MSG_DATA_RESPONSE,        // 数据响应
    MSG_HEARTBEAT,            // 心跳
    MSG_ERROR,                // 错误消息
    MSG_DISCONNECT            // 断开连接
} message_type_t;

// 响应状态
typedef enum {
    STATUS_SUCCESS = 0,       // 成功
    STATUS_ERROR,             // 错误
    STATUS_UPDATE_AVAILABLE,  // 有更新可用
    STATUS_NO_UPDATE,         // 无更新
    STATUS_INVALID_REQUEST,   // 无效请求
    STATUS_SERVER_ERROR       // 服务器错误
} status_code_t;

// 消息头结构
typedef struct {
    uint32_t magic;           // 魔数 0x12345678
    uint16_t version;         // 协议版本
    uint16_t type;            // 消息类型
    uint32_t length;          // 数据长度
    uint32_t checksum;        // 校验和
} __attribute__((packed)) message_header_t;

// 版本检查消息
typedef struct {
    char client_version[32];  // 客户端版本
    char platform[32];        // 平台信息
} __attribute__((packed)) version_check_msg_t;

// 版本响应消息
typedef struct {
    uint16_t status;          // 状态码
    char server_version[32];  // 服务器版本
    char latest_version[32];  // 最新版本
    uint32_t update_size;     // 更新包大小
} __attribute__((packed)) version_response_msg_t;

// 文件上传消息
typedef struct {
    char filename[MAX_FILENAME_LEN];  // 文件名
    uint32_t file_size;               // 文件大小
    uint32_t chunk_size;              // 当前块大小
    uint32_t chunk_offset;            // 块偏移
    char data[];                      // Base64编码的文件数据
} __attribute__((packed)) file_upload_msg_t;

// 数据上传消息
typedef struct {
    char table_name[64];      // 表名
    char field_name[64];      // 字段名
    uint32_t data_size;       // 数据大小
    char data[];              // Base64编码的数据
} __attribute__((packed)) data_upload_msg_t;

// 响应消息
typedef struct {
    uint16_t status;          // 状态码
    char message[MAX_MESSAGE_LEN];  // 响应消息
} __attribute__((packed)) response_msg_t;

// 错误响应消息
typedef struct {
    uint16_t error_code;      // 错误代码
    uint32_t message_len;     // 错误消息长度
    // 错误消息内容跟在结构体后面
} __attribute__((packed)) ErrorResponse;

// 数据响应消息
typedef struct {
    uint16_t status;          // 状态码
    uint32_t data_len;        // 数据长度
    // 数据内容跟在结构体后面
} __attribute__((packed)) DataResponse;

// 文件响应消息
typedef struct {
    uint16_t status;          // 状态码
    uint32_t message_len;     // 消息长度
    // 消息内容跟在结构体后面
} __attribute__((packed)) FileResponse;

// 协议魔数
#define PROTOCOL_MAGIC 0x12345678

// 函数声明
uint32_t calculate_checksum(const void* data, size_t length);
int validate_message_header(const message_header_t* header);
void init_message_header(message_header_t* header, uint16_t type, uint32_t length);

#endif // PROTOCOL_H