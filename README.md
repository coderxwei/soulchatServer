# 分布式即时通讯系统

基于 C++ 与 Node.js 构建的分布式即时通讯后端，支持多服务器水平扩展、跨节点消息推送、好友管理、Redis 分布式锁等特性。

---

## 目录

- [系统架构](#系统架构)
- [技术栈](#技术栈)
- [服务说明](#服务说明)
- [目录结构](#目录结构)
- [环境依赖](#环境依赖)
- [快速开始](#快速开始)
- [配置说明](#配置说明)
- [消息协议](#消息协议)
- [核心设计](#核心设计)

---

## 系统架构

```
客户端
  │
  │  ① HTTP 请求登录，获取 ChatServer 地址
  ▼
GetServer (HTTP)
  │
  │  gRPC：查询负载最低的 ChatServer
  ▼
StatusServer (gRPC)
  │
  │  返回最优 ChatServer 地址
  ▼
GetServer ──返回地址──► 客户端
                           │
                           │  ② TCP 长连接，进行聊天
                           ▼
                   ChatServer A ◄──gRPC──► ChatServer B
                           │
                     Redis / MySQL
```

**通信流程：**
1. 客户端向 `GetServer` 发起 HTTP 登录请求
2. `GetServer` 通过 gRPC 询问 `StatusServer`，获取当前登录人数最少的 `ChatServer` 地址
3. 客户端与分配到的 `ChatServer` 建立 TCP 长连接，进行实时通信
4. 跨服务器消息通过 `ChatServer` 之间的 gRPC 调用转发

---

## 技术栈

| 分类 | 技术 |
|---|---|
| 网络 I/O | Boost.Asio（异步，Reactor 模型）|
| 跨服务通信 | gRPC + Protocol Buffers |
| 缓存 | Redis（hiredis）|
| 数据库 | MySQL（MySQL Connector C++ 8.x）|
| JSON | JsonCpp |
| 邮件服务 | Node.js + Nodemailer |
| 构建工具 | CMake + vcpkg |
| 编译标准 | C++17 |

---

## 服务说明

### VarifyServer（Node.js）
- **职责**：接收 gRPC 请求，向用户邮箱发送验证码，将验证码存入 Redis 并设置过期时间
- **依赖**：`ioredis`、`nodemailer`、`@grpc/grpc-js`

### GetServer（C++，HTTP）
- **职责**：用户注册、登录入口；调用 StatusServer 进行负载均衡，返回 ChatServer 连接地址；操作 MySQL 写入用户数据
- **端口**：HTTP（见 `config.ini`）

### StatusServer（C++，gRPC）
- **职责**：维护各 ChatServer 的在线用户计数（存储于 Redis Hash），响应 GetServer 的负载查询请求，返回负载最低的服务器
- **端口**：gRPC（见 `config.ini`）

### ChatServer（C++，TCP）
- **职责**：维护客户端 TCP 长连接，处理登录鉴权、用户搜索、好友申请/认证、文字消息收发、心跳检测等全部聊天业务
- **扩展**：支持多实例部署，实例间通过 gRPC 互相推送跨服消息
- **端口**：TCP（见 `config.ini`）

---

## 目录结构

```
workSpace/
├── ChatServer/          # 核心聊天服务（C++）
│   ├── include/         # 头文件
│   ├── src/             # 源文件
│   ├── proto/           # Protobuf 定义文件
│   └── CMakeLists.txt
├── GetServer/           # HTTP 网关服务（C++）
│   ├── include/
│   ├── src/
│   └── CMakeLists.txt
├── StatusServer/        # 状态/负载均衡服务（C++）
│   ├── include/
│   ├── src/
│   └── CMakeLists.txt
├── VarifyServer/        # 验证码服务（Node.js）
│   ├── server.js
│   └── package.json
├── generate/            # protoc 生成的 pb 文件
└── README.md
```

---

## 环境依赖

### C++ 服务（GetServer / StatusServer / ChatServer）

| 依赖 | 版本 | 安装方式 |
|---|---|---|
| CMake | ≥ 3.21 | 官网下载 |
| vcpkg | 最新 | `git clone https://github.com/microsoft/vcpkg` |
| Boost | ≥ 1.80 | `vcpkg install boost` |
| gRPC | ≥ 1.50 | `vcpkg install grpc` |
| protobuf | ≥ 3.21 | `vcpkg install protobuf` |
| hiredis | 最新 | `vcpkg install hiredis` |
| JsonCpp | 最新 | `vcpkg install jsoncpp` |
| MySQL Connector C++ | 8.x | 官网下载，配置路径见下方 |

### Node.js 服务（VarifyServer）

| 依赖 | 版本 |
|---|---|
| Node.js | ≥ 18 |
| npm | ≥ 9 |

### 中间件

| 服务 | 版本 |
|---|---|
| Redis | ≥ 6.0 |
| MySQL | ≥ 8.0 |

---

## 快速开始

### 1. 启动中间件

```bash
# 启动 Redis
redis-server

# 启动 MySQL，并初始化数据库
mysql -u root -p < schema.sql
```

### 2. 配置各服务

将各服务目录下的 `config.ini.example` 复制为 `config.ini` 并填写实际配置：

```bash
cp ChatServer/config.ini.example ChatServer/config.ini
cp GetServer/config.ini.example  GetServer/config.ini
cp StatusServer/config.ini.example StatusServer/config.ini
```

### 3. 构建 C++ 服务

```bash
# 以 ChatServer 为例，其余服务同理
cd ChatServer
cmake -B cmake-build-release \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=<vcpkg路径>/scripts/buildsystems/vcpkg.cmake
cmake --build cmake-build-release --config Release
```

### 4. 启动 VarifyServer

```bash
cd VarifyServer
npm install
npm run serve
```

### 5. 按顺序启动 C++ 服务

```bash
# 1. StatusServer
./StatusServer/cmake-build-release/StatusServer

# 2. ChatServer（可启动多个实例）
./ChatServer/cmake-build-release/ChatServer

# 3. GetServer
./GetServer/cmake-build-release/GetServer
```

---

## 配置说明

各服务的 `config.ini` 格式如下（以 ChatServer 为例）：

```ini
[SelfServer]
Name  = chatserver1      # 当前服务器唯一名称（多实例时需不同）
Port  = 8080

[Redis]
Host   = 127.0.0.1
Port   = 6379
Passwd = your_redis_password

[MySQL]
Host   = 127.0.0.1
Port   = 3306
User   = root
Passwd = your_mysql_password
Schema = chatdb

[StatusServer]
Host = 127.0.0.1
Port = 50051             # StatusServer 的 gRPC 端口
```

---

## 消息协议

客户端与 ChatServer 之间使用**自定义二进制协议**，消息格式：

```
┌──────────────┬──────────────┬─────────────────────────┐
│  MsgId (2B)  │ DataLen (2B) │     Body (DataLen B)     │
│  网络字节序   │  网络字节序  │       JSON 字符串         │
└──────────────┴──────────────┴─────────────────────────┘
```

主要消息 ID：

| ID | 方向 | 说明 |
|---|---|---|
| 1005 | C→S | 用户登录 |
| 1006 | S→C | 登录响应 |
| 1007 | C→S | 搜索用户 |
| 1009 | C→S | 申请添加好友 |
| 1011 | S→C | 通知收到好友申请 |
| 1013 | C→S | 认证好友 |
| 1015 | S→C | 通知好友认证通过 |
| 1017 | C→S | 发送文字消息 |
| 1019 | S→C | 通知收到文字消息 |
| 1021 | S→C | 通知下线（被踢） |
| 1023 | C→S | 心跳请求 |
| 1024 | S→C | 心跳响应 |

---

## 核心设计

### 异步网络模型
采用 `Boost.Asio` + `IO Service Pool`，多个 `io_context` 分配到独立线程，新连接以 round-robin 方式分配，充分利用多核。业务逻辑由 `LogicSystem` 单独线程通过消息队列处理，I/O 与业务完全解耦。

### 粘包处理
固定 4 字节消息头（2B msgId + 2B length），`asyncReadFull()` 内部递归确保精确读取指定字节数，从协议层解决粘包/拆包问题。

### Redis 分布式锁
用户登录和连接断开时，使用 `SET key uuid NX EX` 加锁，Lua 脚本原子比较后释放，防止多端并发登录导致状态不一致。

### 跨服务器推送
消息发送时，从 Redis 查询接收方所在服务器（`uip_<uid>`）；同服务器直接推送，跨服务器通过 gRPC 调用目标 ChatServer 的 `NotifyTextChatMsg` 转发。

### 心跳检测
服务端定时器每 60 秒扫描所有 `CSession`，对超时未收到心跳的连接主动关闭，并触发异常断开处理流程。
