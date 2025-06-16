# Base64网络传输系统使用文档

## 目录
- [系统概述](#系统概述)
- [环境要求](#环境要求)
- [安装部署](#安装部署)
- [服务端使用](#服务端使用)
- [客户端使用](#客户端使用)
- [配置说明](#配置说明)
- [API参考](#api参考)

## 系统概述

Base64网络传输系统是一个基于Linux的C语言实现的客户端-服务端通信系统，主要功能包括：

- **TCP通信**: 客户端和服务端之间的可靠TCP连接
- **多线程支持**: 客户端多线程架构，支持并发操作
- **自动更新**: 客户端启动时自动检查并下载更新
- **Base64编码**: 所有传输数据使用Base64编码
- **数据存储**: 文件直接存储，字段数据存储到SQLite数据库
- **图形界面**: 基于GTK的中文图形界面
- **版本控制**: 使用Git进行代码版本管理

## 环境要求

### 操作系统
- Linux (Ubuntu 20.04+ 推荐)
- 支持GTK 3.0+的桌面环境

### 系统依赖
```bash
# 编译工具
sudo apt install build-essential gcc make cmake

# 系统库
sudo apt install libc6-dev linux-libc-dev

# SQLite数据库
sudo apt install libsqlite3-dev sqlite3

# GTK图形界面库
sudo apt install libgtk-3-dev pkg-config

# 开发工具
sudo apt install git vim gdb valgrind
```

或者使用项目提供的快速安装命令：
```bash
make install-deps
```

## 安装部署


```bash
# 安装依赖
make install-deps

# 编译所有组件
make all

# 或者分别编译
make server    # 编译服务端
make client    # 编译客户端
```

```

## 服务端使用

### 启动服务端
```bash
# 使用默认配置启动
./build/server

# 指定端口启动
./build/server -p 8080

# 后台运行
nohup ./build/server > server.log 2>&1 &
```

### 服务端命令行参数
```bash
./build/server [选项]

选项:
  -p, --port PORT     指定监听端口 (默认: 8888)
  -h, --help          显示帮助信息
  -v, --version       显示版本信息
  -d, --daemon        后台运行模式
```

### 服务端状态监控
服务端运行时会显示实时状态信息：
- 当前连接的客户端数量
- 服务器运行时间
- 处理的消息统计
- 内存使用情况

### 停止服务端
```bash
# 优雅停止
kill -TERM <server_pid>

# 或者使用Ctrl+C (前台运行时)
```

## 客户端使用

### 图形界面模式
```bash
# 启动GUI客户端
./build/client

# 或者明确指定GUI模式
./build/client --gui
```

#### GUI操作说明

1. **连接服务器**
   - 在"服务器地址"输入框中输入服务器IP地址
   - 在"端口"输入框中输入服务器端口
   - 点击"连接"按钮建立连接

2. **文件上传**
   - 点击"选择文件"按钮选择要上传的文件
   - 点击"上传文件"按钮开始上传
   - 进度条显示上传进度

3. **数据上传**
   - 在"表名"输入框中输入数据库表名
   - 在"字段名"输入框中输入字段名
   - 在"数据"输入框中输入要上传的数据
   - 点击"上传数据"按钮提交数据

4. **查看日志**
   - 操作日志区域显示所有操作的详细信息
   - 包括连接状态、上传进度、错误信息等



```
可用命令:
  connect <host> <port>  - 连接到服务器
  disconnect             - 断开连接
  upload <filepath>      - 上传文件
  data <table> <field> <value> - 上传数据
  status                 - 显示连接状态
  update                 - 检查更新
  config                 - 显示配置
  help                   - 显示帮助
  quit                   - 退出程序
```

### 客户端命令行参数
```bash
./build/client [选项]

选项:
  --gui                   启动图形界面模式 (默认)
  --cli                   启动命令行模式
  -h, --host HOST         服务器地址 (默认: localhost)
  -p, --port PORT         服务器端口 (默认: 8888)
  --no-auto-update        禁用自动更新
  --heartbeat SECONDS     心跳间隔 (默认: 30秒)
  --timeout SECONDS       连接超时 (默认: 30秒)
  --log-level LEVEL       日志级别 (0-3, 默认: 1)
  --help                  显示帮助信息
  --version               显示版本信息
```

## 配置说明

### 客户端配置文件
客户端配置文件 `client.conf` 会在首次运行时自动创建：

```ini
# Base64网络传输客户端配置文件

# 服务器配置
server_host=localhost
server_port=8888

# 客户端行为配置
auto_update=true
heartbeat_interval=30
connection_timeout=30
max_retry_count=3

# 日志级别 (0=DEBUG, 1=INFO, 2=WARN, 3=ERROR)
log_level=1
```

### 配置项说明

| 配置项 | 说明 | 默认值 | 范围 |
|--------|------|--------|------|
| server_host | 服务器地址 | localhost | 有效的IP地址或域名 |
| server_port | 服务器端口 | 8888 | 1-65535 |
| auto_update | 自动更新 | true | true/false |
| heartbeat_interval | 心跳间隔(秒) | 30 | 5-300 |
| connection_timeout | 连接超时(秒) | 30 | 5-120 |
| max_retry_count | 最大重试次数 | 3 | 0-10 |
| log_level | 日志级别 | 1 | 0-3 |



#### 启用详细日志
```bash
# 客户端调试模式
./build/client --log-level 0

# 服务端调试模式
./build/server -v
```

#### 内存检查
```bash
# 使用Valgrind检查内存泄漏
valgrind --leak-check=full ./build/client
```

### 日志文件

- 服务端日志: `server.log`
- 客户端日志: `logs/client.log`
- 数据库日志: 存储在SQLite数据库中

## API参考

### 消息协议

#### 消息头格式
```c
typedef struct {
    uint32_t magic;        // 魔数: 0x12345678
    uint32_t version;      // 协议版本
    uint32_t type;         // 消息类型
    uint32_t length;       // 数据长度
    uint32_t checksum;     // 校验和
    uint64_t timestamp;    // 时间戳
} MessageHeader;
```

#### 消息类型

| 类型 | 值 | 说明 |
|------|----|----- |
| MSG_VERSION_CHECK | 1 | 版本检查 |
| MSG_UPDATE_REQUEST | 2 | 更新请求 |
| MSG_FILE_UPLOAD | 3 | 文件上传 |
| MSG_DATA_UPLOAD | 4 | 数据上传 |
| MSG_HEARTBEAT | 5 | 心跳消息 |
| MSG_VERSION_RESPONSE | 101 | 版本响应 |
| MSG_UPDATE_DATA | 102 | 更新数据 |
| MSG_FILE_RESPONSE | 103 | 文件响应 |
| MSG_DATA_RESPONSE | 104 | 数据响应 |
| MSG_ERROR_RESPONSE | 105 | 错误响应 |
| MSG_HEARTBEAT_RESPONSE | 106 | 心跳响应 |

#### 状态码

| 状态码 | 说明 |
|--------|------|
| STATUS_SUCCESS | 成功 |
| STATUS_ERROR | 一般错误 |
| STATUS_INVALID_MESSAGE | 无效消息 |
| STATUS_FILE_NOT_FOUND | 文件未找到 |
| STATUS_PERMISSION_DENIED | 权限拒绝 |
| STATUS_DISK_FULL | 磁盘空间不足 |
| STATUS_DATABASE_ERROR | 数据库错误 |
| STATUS_NETWORK_ERROR | 网络错误 |

### 数据库结构

#### 客户端连接表 (clients)
```sql
CREATE TABLE clients (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    client_id TEXT NOT NULL,
    ip_address TEXT NOT NULL,
    connect_time INTEGER NOT NULL,
    disconnect_time INTEGER,
    version TEXT,
    status TEXT DEFAULT 'connected'
);
```

#### 文件上传表 (file_uploads)
```sql
CREATE TABLE file_uploads (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    client_id TEXT NOT NULL,
    filename TEXT NOT NULL,
    file_size INTEGER NOT NULL,
    upload_time INTEGER NOT NULL,
    file_path TEXT NOT NULL,
    checksum TEXT
);
```

#### 字段数据表 (field_data)
```sql
CREATE TABLE field_data (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    client_id TEXT NOT NULL,
    table_name TEXT NOT NULL,
    field_name TEXT NOT NULL,
    field_value TEXT NOT NULL,
    upload_time INTEGER NOT NULL
);
```

#### 系统日志表 (system_logs)
```sql
CREATE TABLE system_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER NOT NULL,
    level TEXT NOT NULL,
    message TEXT NOT NULL,
    client_id TEXT
);
```

#### 版本信息表 (version_info)
```sql
CREATE TABLE version_info (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    version TEXT NOT NULL,
    release_date INTEGER NOT NULL,
    file_path TEXT,
    file_size INTEGER,
    description TEXT
);
```

### 文件结构

```
base64-network-transfer/
├── src/                    # 源代码目录
│   ├── client/            # 客户端源码
│   │   ├── main.c         # 客户端主程序
│   │   ├── network.c      # 网络通信
│   │   ├── gui.c          # 图形界面
│   │   ├── update.c       # 自动更新
│   │   ├── file_handler.c # 文件处理
│   │   ├── config.c       # 配置管理
│   │   └── client.h       # 客户端头文件
│   ├── server/            # 服务端源码
│   │   ├── main.c         # 服务端主程序
│   │   ├── network.c      # 网络服务
│   │   ├── database.c     # 数据库操作
│   │   ├── file_handler.c # 文件处理
│   │   ├── message_handler.c # 消息处理
│   │   └── server.h       # 服务端头文件
│   └── common/            # 公共代码
│       ├── protocol.h     # 通信协议
│       ├── base64.h       # Base64编码
│       ├── base64.c       # Base64实现
│       ├── utils.h        # 工具函数
│       └── utils.c        # 工具实现
├── build/                 # 编译输出目录
├── data/                  # 服务端数据目录
│   ├── database/          # 数据库文件
│   └── uploads/           # 上传文件
├── docs/                  # 文档目录
├── downloads/             # 客户端下载目录
├── temp/                  # 临时文件目录
├── update/                # 更新文件目录
├── logs/                  # 日志文件目录
├── Makefile              # 构建脚本
├── README.md             # 项目说明
├── requirements.txt      # 依赖说明
├── .gitignore           # Git忽略文件
└── client.conf          # 客户端配置文件
