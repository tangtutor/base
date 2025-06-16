# Base64网络传输系统 API 文档

## 目录
- [协议概述](#协议概述)
- [消息格式](#消息格式)
- [消息类型](#消息类型)
- [客户端API](#客户端api)
- [服务端API](#服务端api)
- [数据库API](#数据库api)
- [工具函数API](#工具函数api)
- [错误码](#错误码)

## 协议概述

Base64网络传输系统使用自定义的二进制协议进行客户端和服务端之间的通信。所有数据传输都经过Base64编码，确保数据的安全性和完整性。

### 协议特性
- **二进制协议**: 高效的数据传输
- **校验和验证**: 确保数据完整性
- **版本控制**: 支持协议版本兼容性
- **时间戳**: 记录消息发送时间
- **Base64编码**: 所有用户数据都经过Base64编码

## 消息格式

### 消息头结构

```c
typedef struct {
    uint32_t magic;        // 魔数: 0x12345678
    uint32_t version;      // 协议版本: 当前为1
    uint32_t type;         // 消息类型
    uint32_t length;       // 数据长度(不包括消息头)
    uint32_t checksum;     // 数据校验和
    uint64_t timestamp;    // Unix时间戳
} MessageHeader;
```

**字段说明:**
- `magic`: 固定魔数，用于识别协议
- `version`: 协议版本号，用于兼容性检查
- `type`: 消息类型，定义消息的用途
- `length`: 消息体长度，不包括消息头
- `checksum`: 消息体的CRC32校验和
- `timestamp`: 消息发送的Unix时间戳

### 消息传输格式

```
[MessageHeader(32字节)] + [MessageBody(length字节)]
```

## 消息类型

### 客户端发送的消息

| 消息类型 | 值 | 说明 | 数据结构 |
|----------|----|----- |----------|
| MSG_VERSION_CHECK | 1 | 版本检查请求 | VersionCheckMessage |
| MSG_UPDATE_REQUEST | 2 | 更新请求 | UpdateRequestMessage |
| MSG_FILE_UPLOAD | 3 | 文件上传 | FileUploadMessage |
| MSG_DATA_UPLOAD | 4 | 数据上传 | DataUploadMessage |
| MSG_HEARTBEAT | 5 | 心跳消息 | HeartbeatMessage |

### 服务端发送的消息

| 消息类型 | 值 | 说明 | 数据结构 |
|----------|----|----- |----------|
| MSG_VERSION_RESPONSE | 101 | 版本检查响应 | VersionResponse |
| MSG_UPDATE_DATA | 102 | 更新数据 | UpdateData |
| MSG_FILE_RESPONSE | 103 | 文件上传响应 | FileResponse |
| MSG_DATA_RESPONSE | 104 | 数据上传响应 | DataResponse |
| MSG_ERROR_RESPONSE | 105 | 错误响应 | ErrorResponse |
| MSG_HEARTBEAT_RESPONSE | 106 | 心跳响应 | HeartbeatResponse |

### 消息数据结构

#### 版本检查消息 (MSG_VERSION_CHECK)
```c
typedef struct {
    char client_version[32];  // 客户端版本号
    char client_id[64];       // 客户端唯一标识
} VersionCheckMessage;
```

#### 版本响应 (MSG_VERSION_RESPONSE)
```c
typedef struct {
    char server_version[32];   // 服务端版本号
    char latest_version[32];   // 最新客户端版本号
    uint32_t update_available; // 是否有更新可用 (0/1)
    uint32_t update_size;      // 更新文件大小
} VersionResponse;
```

#### 更新请求 (MSG_UPDATE_REQUEST)
```c
typedef struct {
    char client_version[32];  // 当前客户端版本
} UpdateRequestMessage;
```

#### 更新数据 (MSG_UPDATE_DATA)
```c
typedef struct {
    char filename[256];       // 更新文件名
    uint32_t file_size;       // 文件大小
    uint32_t chunk_index;     // 分块索引
    uint32_t total_chunks;    // 总分块数
    uint32_t chunk_size;      // 当前分块大小
    // 后跟Base64编码的文件数据
} UpdateData;
```

#### 文件上传 (MSG_FILE_UPLOAD)
```c
typedef struct {
    uint32_t filename_len;    // 文件名长度
    uint32_t file_size;       // 文件大小(Base64编码后)
    // 后跟: filename + Base64编码的文件数据
} FileUploadMessage;
```

#### 文件响应 (MSG_FILE_RESPONSE)
```c
typedef struct {
    uint32_t status;          // 状态码
    uint32_t message_len;     // 消息长度
    // 后跟: 状态消息
} FileResponse;
```

#### 数据上传 (MSG_DATA_UPLOAD)
```c
typedef struct {
    uint32_t table_name_len;  // 表名长度
    uint32_t field_name_len;  // 字段名长度
    uint32_t data_len;        // 数据长度(Base64编码后)
    // 后跟: table_name + field_name + Base64编码的数据
} DataUploadMessage;
```

#### 数据响应 (MSG_DATA_RESPONSE)
```c
typedef struct {
    uint32_t status;          // 状态码
    uint32_t message_len;     // 消息长度
    // 后跟: 状态消息
} DataResponse;
```

#### 心跳消息 (MSG_HEARTBEAT)
```c
typedef struct {
    uint64_t client_time;     // 客户端时间戳
    uint32_t sequence;        // 序列号
} HeartbeatMessage;
```

#### 心跳响应 (MSG_HEARTBEAT_RESPONSE)
```c
typedef struct {
    uint64_t server_time;     // 服务端时间戳
    uint32_t sequence;        // 对应的序列号
} HeartbeatResponse;
```

#### 错误响应 (MSG_ERROR_RESPONSE)
```c
typedef struct {
    uint32_t error_code;      // 错误码
    uint32_t message_len;     // 错误消息长度
    // 后跟: 错误消息
} ErrorResponse;
```

## 客户端API

### 网络通信API

#### client_init
```c
int client_init();
```
**功能**: 初始化客户端
**返回值**: 成功返回0，失败返回-1

#### client_cleanup
```c
void client_cleanup();
```
**功能**: 清理客户端资源

#### client_connect
```c
int client_connect(const char* host, int port);
```
**功能**: 连接到服务器
**参数**:
- `host`: 服务器地址
- `port`: 服务器端口
**返回值**: 成功返回0，失败返回-1

#### client_disconnect
```c
void client_disconnect();
```
**功能**: 断开与服务器的连接

#### send_message
```c
int send_message(uint32_t type, const void* data, size_t data_len);
```
**功能**: 发送消息到服务器
**参数**:
- `type`: 消息类型
- `data`: 消息数据
- `data_len`: 数据长度
**返回值**: 成功返回0，失败返回-1

#### receive_message
```c
int receive_message(MessageHeader* header, char** data);
```
**功能**: 接收服务器消息
**参数**:
- `header`: 消息头结构体指针
- `data`: 接收数据的指针(需要调用者释放)
**返回值**: 成功返回0，失败返回-1

### 文件处理API

#### upload_file
```c
int upload_file(const char* filepath);
```
**功能**: 上传文件到服务器
**参数**:
- `filepath`: 本地文件路径
**返回值**: 成功返回0，失败返回-1

#### send_file_upload
```c
int send_file_upload(const char* filename, const char* file_data, size_t data_len);
```
**功能**: 发送文件上传消息
**参数**:
- `filename`: 文件名
- `file_data`: Base64编码的文件数据
- `data_len`: 数据长度
**返回值**: 成功返回0，失败返回-1

#### send_data_upload
```c
int send_data_upload(const char* table_name, const char* field_name, const char* data);
```
**功能**: 发送数据上传消息
**参数**:
- `table_name`: 数据库表名
- `field_name`: 字段名
- `data`: 要上传的数据
**返回值**: 成功返回0，失败返回-1

### 更新API

#### send_version_check
```c
int send_version_check();
```
**功能**: 发送版本检查请求
**返回值**: 成功返回0，失败返回-1

#### send_update_request
```c
int send_update_request();
```
**功能**: 发送更新请求
**返回值**: 成功返回0，失败返回-1

#### check_for_updates
```c
int check_for_updates();
```
**功能**: 检查是否有可用更新
**返回值**: 有更新返回1，无更新返回0，错误返回-1

#### download_update
```c
int download_update(const char* filename, const char* data, size_t data_len);
```
**功能**: 下载更新文件
**参数**:
- `filename`: 更新文件名
- `data`: Base64编码的文件数据
- `data_len`: 数据长度
**返回值**: 成功返回0，失败返回-1

### 配置API

#### load_config
```c
int load_config(ClientConfig* config);
```
**功能**: 加载客户端配置
**参数**:
- `config`: 配置结构体指针
**返回值**: 成功返回0，失败返回-1

#### save_config
```c
int save_config(const ClientConfig* config);
```
**功能**: 保存客户端配置
**参数**:
- `config`: 配置结构体指针
**返回值**: 成功返回0，失败返回-1

### GUI API

#### gui_init
```c
int gui_init(int argc, char* argv[]);
```
**功能**: 初始化图形界面
**参数**:
- `argc`: 命令行参数个数
- `argv`: 命令行参数数组
**返回值**: 成功返回0，失败返回-1

#### gui_run
```c
void gui_run();
```
**功能**: 运行GUI主循环

#### gui_update_status
```c
void gui_update_status(const char* status);
```
**功能**: 更新状态显示
**参数**:
- `status`: 状态文本

#### gui_log_message
```c
void gui_log_message(const char* message);
```
**功能**: 在GUI中记录日志消息
**参数**:
- `message`: 日志消息

#### gui_set_progress
```c
void gui_set_progress(double progress);
```
**功能**: 设置进度条
**参数**:
- `progress`: 进度值(0.0-1.0)

## 服务端API

### 服务器管理API

#### server_init
```c
int server_init(int port);
```
**功能**: 初始化服务器
**参数**:
- `port`: 监听端口
**返回值**: 成功返回0，失败返回-1

#### server_start
```c
int server_start();
```
**功能**: 启动服务器
**返回值**: 成功返回0，失败返回-1

#### server_stop
```c
void server_stop();
```
**功能**: 停止服务器

#### server_cleanup
```c
void server_cleanup();
```
**功能**: 清理服务器资源

### 客户端管理API

#### accept_client
```c
int accept_client();
```
**功能**: 接受新的客户端连接
**返回值**: 成功返回客户端索引，失败返回-1

#### disconnect_client
```c
void disconnect_client(int client_index);
```
**功能**: 断开客户端连接
**参数**:
- `client_index`: 客户端索引

#### get_client_count
```c
int get_client_count();
```
**功能**: 获取当前连接的客户端数量
**返回值**: 客户端数量

### 消息处理API

#### handle_version_check
```c
void handle_version_check(int client_index, const char* data, size_t data_len);
```
**功能**: 处理版本检查消息
**参数**:
- `client_index`: 客户端索引
- `data`: 消息数据
- `data_len`: 数据长度

#### handle_update_request
```c
void handle_update_request(int client_index, const char* data, size_t data_len);
```
**功能**: 处理更新请求消息
**参数**:
- `client_index`: 客户端索引
- `data`: 消息数据
- `data_len`: 数据长度

#### handle_file_upload
```c
void handle_file_upload(int client_index, const char* data, size_t data_len);
```
**功能**: 处理文件上传消息
**参数**:
- `client_index`: 客户端索引
- `data`: 消息数据
- `data_len`: 数据长度

#### handle_data_upload
```c
void handle_data_upload(int client_index, const char* data, size_t data_len);
```
**功能**: 处理数据上传消息
**参数**:
- `client_index`: 客户端索引
- `data`: 消息数据
- `data_len`: 数据长度

### 文件处理API

#### save_uploaded_file
```c
int save_uploaded_file(const char* filename, const char* data, size_t data_len, const char* client_id);
```
**功能**: 保存上传的文件
**参数**:
- `filename`: 文件名
- `data`: Base64编码的文件数据
- `data_len`: 数据长度
- `client_id`: 客户端ID
**返回值**: 成功返回0，失败返回-1

#### send_file_response
```c
void send_file_response(int client_index, uint32_t status, const char* message);
```
**功能**: 发送文件响应
**参数**:
- `client_index`: 客户端索引
- `status`: 状态码
- `message`: 响应消息

#### send_update_file
```c
int send_update_file(int client_index, const char* filepath);
```
**功能**: 发送更新文件
**参数**:
- `client_index`: 客户端索引
- `filepath`: 更新文件路径
**返回值**: 成功返回0，失败返回-1

## 数据库API

### 数据库管理

#### database_init
```c
int database_init(const char* db_path);
```
**功能**: 初始化数据库
**参数**:
- `db_path`: 数据库文件路径
**返回值**: 成功返回0，失败返回-1

#### database_cleanup
```c
void database_cleanup();
```
**功能**: 清理数据库资源

#### create_tables
```c
int create_tables();
```
**功能**: 创建数据库表
**返回值**: 成功返回0，失败返回-1

### 数据操作

#### store_field_data
```c
int store_field_data(const char* client_id, const char* table_name, 
                     const char* field_name, const char* field_value);
```
**功能**: 存储字段数据
**参数**:
- `client_id`: 客户端ID
- `table_name`: 表名
- `field_name`: 字段名
- `field_value`: 字段值
**返回值**: 成功返回0，失败返回-1

#### log_client_connection
```c
int log_client_connection(const char* client_id, const char* ip_address, const char* version);
```
**功能**: 记录客户端连接
**参数**:
- `client_id`: 客户端ID
- `ip_address`: IP地址
- `version`: 客户端版本
**返回值**: 成功返回0，失败返回-1

#### log_file_upload
```c
int log_file_upload(const char* client_id, const char* filename, 
                    size_t file_size, const char* file_path);
```
**功能**: 记录文件上传
**参数**:
- `client_id`: 客户端ID
- `filename`: 文件名
- `file_size`: 文件大小
- `file_path`: 文件路径
**返回值**: 成功返回0，失败返回-1

#### get_latest_version
```c
int get_latest_version(char* version, size_t version_len, char* file_path, 
                       size_t path_len, size_t* file_size);
```
**功能**: 获取最新版本信息
**参数**:
- `version`: 版本号缓冲区
- `version_len`: 版本号缓冲区大小
- `file_path`: 文件路径缓冲区
- `path_len`: 文件路径缓冲区大小
- `file_size`: 文件大小指针
**返回值**: 成功返回0，失败返回-1

## 工具函数API

### Base64编码

#### base64_encode
```c
int base64_encode(const unsigned char* input, size_t input_len, char* output);
```
**功能**: Base64编码
**参数**:
- `input`: 输入数据
- `input_len`: 输入数据长度
- `output`: 输出缓冲区
**返回值**: 成功返回0，失败返回-1

#### base64_decode
```c
int base64_decode(const char* input, unsigned char* output, size_t* output_len);
```
**功能**: Base64解码
**参数**:
- `input`: 输入的Base64字符串
- `output`: 输出缓冲区
- `output_len`: 输出数据长度指针
**返回值**: 成功返回0，失败返回-1

#### base64_encoded_length
```c
size_t base64_encoded_length(size_t input_len);
```
**功能**: 计算Base64编码后的长度
**参数**:
- `input_len`: 输入数据长度
**返回值**: 编码后的长度

#### base64_decoded_length
```c
size_t base64_decoded_length(const char* input);
```
**功能**: 计算Base64解码后的长度
**参数**:
- `input`: Base64字符串
**返回值**: 解码后的长度

### 工具函数

#### calculate_checksum
```c
uint32_t calculate_checksum(const void* data, size_t len, uint32_t initial);
```
**功能**: 计算CRC32校验和
**参数**:
- `data`: 数据指针
- `len`: 数据长度
- `initial`: 初始值
**返回值**: 校验和

#### get_timestamp
```c
uint64_t get_timestamp();
```
**功能**: 获取当前Unix时间戳
**返回值**: 时间戳

#### create_directory
```c
int create_directory(const char* path);
```
**功能**: 创建目录
**参数**:
- `path`: 目录路径
**返回值**: 成功返回0，失败返回-1

#### file_exists
```c
int file_exists(const char* filepath);
```
**功能**: 检查文件是否存在
**参数**:
- `filepath`: 文件路径
**返回值**: 存在返回1，不存在返回0

#### get_file_size
```c
long get_file_size(const char* filepath);
```
**功能**: 获取文件大小
**参数**:
- `filepath`: 文件路径
**返回值**: 文件大小，失败返回-1

#### extract_filename
```c
const char* extract_filename(const char* filepath);
```
**功能**: 从路径中提取文件名
**参数**:
- `filepath`: 文件路径
**返回值**: 文件名指针

#### generate_random_string
```c
void generate_random_string(char* buffer, size_t length);
```
**功能**: 生成随机字符串
**参数**:
- `buffer`: 输出缓冲区
- `length`: 字符串长度

## 错误码

### 状态码定义

| 状态码 | 值 | 说明 |
|--------|----|----- |
| STATUS_SUCCESS | 0 | 操作成功 |
| STATUS_ERROR | 1 | 一般错误 |
| STATUS_INVALID_MESSAGE | 2 | 无效消息 |
| STATUS_FILE_NOT_FOUND | 3 | 文件未找到 |
| STATUS_PERMISSION_DENIED | 4 | 权限拒绝 |
| STATUS_DISK_FULL | 5 | 磁盘空间不足 |
| STATUS_DATABASE_ERROR | 6 | 数据库错误 |
| STATUS_NETWORK_ERROR | 7 | 网络错误 |
| STATUS_INVALID_CHECKSUM | 8 | 校验和错误 |
| STATUS_VERSION_MISMATCH | 9 | 版本不匹配 |
| STATUS_CLIENT_LIMIT_REACHED | 10 | 客户端连接数达到上限 |

### 错误处理

所有API函数都遵循以下错误处理约定：

1. **返回值**: 成功返回0或正值，失败返回-1或负值
2. **错误日志**: 错误信息会记录到日志系统
3. **资源清理**: 发生错误时自动清理已分配的资源
4. **错误传播**: 底层错误会向上层传播

### 调试支持

#### 日志级别

| 级别 | 值 | 说明 |
|------|----|----- |
| LOG_DEBUG | 0 | 调试信息 |
| LOG_INFO | 1 | 一般信息 |
| LOG_WARN | 2 | 警告信息 |
| LOG_ERROR | 3 | 错误信息 |

#### 调试函数

```c
void log_message(int level, const char* format, ...);
void dump_message_header(const MessageHeader* header);
void dump_hex(const void* data, size_t len);
```

## 使用示例

### 客户端连接示例

```c
#include "client.h"

int main() {
    // 初始化客户端
    if (client_init() != 0) {
        printf("客户端初始化失败\n");
        return -1;
    }
    
    // 连接到服务器
    if (client_connect("localhost", 8888) != 0) {
        printf("连接服务器失败\n");
        client_cleanup();
        return -1;
    }
    
    // 发送版本检查
    if (send_version_check() != 0) {
        printf("版本检查失败\n");
    }
    
    // 上传文件
    if (upload_file("test.txt") != 0) {
        printf("文件上传失败\n");
    }
    
    // 上传数据
    if (send_data_upload("users", "name", "张三") != 0) {
        printf("数据上传失败\n");
    }
    
    // 断开连接
    client_disconnect();
    client_cleanup();
    
    return 0;
}
```

### 服务端启动示例

```c
#include "server.h"

int main() {
    // 初始化服务器
    if (server_init(8888) != 0) {
        printf("服务器初始化失败\n");
        return -1;
    }
    
    // 初始化数据库
    if (database_init("data/database/server.db") != 0) {
        printf("数据库初始化失败\n");
        server_cleanup();
        return -1;
    }
    
    // 启动服务器
    if (server_start() != 0) {
        printf("服务器启动失败\n");
        database_cleanup();
        server_cleanup();
        return -1;
    }
    
    printf("服务器运行中，按Ctrl+C停止...\n");
    
    // 主循环
    while (g_server.running) {
        sleep(1);
    }
    
    // 清理资源
    server_stop();
    database_cleanup();
    server_cleanup();
    
    return 0;
}
```

---

