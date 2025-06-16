# Base64编码网络传输系统

## 项目概述

这是一个基于Linux的C语言实现的客户端-服务端通信系统，支持Base64编码数据传输、自动更新、多线程处理和图形界面。

## 功能特性

1. **TCP网络通信**: 客户端与服务端之间的可靠TCP连接
2. **多线程架构**: 主线程处理用户交互，工作线程处理网络通信
3. **自动更新**: 客户端启动时检查服务端版本并自动下载更新
4. **Base64编码**: 所有上传数据使用Base64编码传输
5. **数据存储**: 文件直接存储，字段数据使用SQLite数据库
6. **图形界面**: 基于GTK的中文用户界面
7. **版本控制**: 使用Git进行代码管理

## 项目结构

```
base/
├── README.md                 # 项目说明文档
├── docs/                     # 文档目录
│   ├── USAGE.md             # 使用说明
│   └── API.md               # API文档
├── src/                     # 源代码目录
│   ├── client/              # 客户端代码
│   │   ├── main.c           # 客户端主程序
│   │   ├── client.h         # 客户端头文件
│   │   ├── network.c        # 网络通信模块
│   │   ├── base64.c         # Base64编码模块
│   │   ├── update.c         # 自动更新模块
│   │   └── gui.c            # 图形界面模块
│   ├── server/              # 服务端代码
│   │   ├── main.c           # 服务端主程序
│   │   ├── server.h         # 服务端头文件
│   │   ├── network.c        # 网络处理模块
│   │   ├── database.c       # 数据库操作模块
│   │   └── file_handler.c   # 文件处理模块
│   └── common/              # 共用代码
│       ├── base64.h         # Base64编码头文件
│       ├── protocol.h       # 通信协议定义
│       └── utils.c          # 工具函数
├── build/                   # 编译输出目录
├── data/                    # 数据目录
│   ├── database/            # 数据库文件
│   └── uploads/             # 上传文件存储
├── Makefile                 # 编译配置
└── .gitignore              # Git忽略文件
```

## 编译和运行

### 依赖项

- GCC编译器
- SQLite3开发库
- GTK+3开发库
- pthread库

### 安装依赖

```bash
sudo apt-get update
sudo apt-get install build-essential libsqlite3-dev libgtk-3-dev
```

### 编译

```bash
make all
```

### 运行

1. 启动服务端：
```bash
./build/server
```

2. 启动客户端：
```bash
./build/client
```

