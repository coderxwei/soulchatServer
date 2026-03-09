# ChatServer

分布式聊天服务器，基于 C++17 + Boost.Asio + gRPC + MySQL + Redis 构建。

## 项目结构

```
ChatServer_2/
├── CMakeLists.txt          # CMake 构建文件（CLion 使用）
├── config.ini              # 运行时配置文件
├── README.md
├── include/                # 所有头文件
│   ├── const.h             # 全局常量、错误码、消息 ID、Defer 工具类
│   ├── data.h              # 数据结构（UserInfo、ApplyInfo）
│   ├── Singleton.h         # 线程安全单例模板
│   ├── AsioIOServicePool.h # Boost.Asio io_context 线程池
│   ├── MsgNode.h           # 消息节点（MsgNode / RecvNode / SendNode）
│   ├── CSession.h          # 客户端会话（TCP 连接抽象）
│   ├── CServer.h           # TCP 服务器（连接接受 + 心跳定时器）
│   ├── LogicSystem.h       # 消息分发与业务逻辑处理
│   ├── UserMgr.h           # uid ↔ CSession 映射管理
│   ├── ConfigMgr.h         # INI 配置文件解析器
│   ├── RedisMgr.h          # Redis 连接池 + 操作封装
│   ├── DistLock.h          # 基于 Redis 的分布式锁
│   ├── MysqlDao.h          # MySQL 连接池 + DAO 操作
│   ├── MysqlMgr.h          # MySQL 单例管理器
│   ├── ChatGrpcClient.h    # 跨服务器 gRPC 客户端（ChatService）
│   ├── ChatServiceImpl.h   # gRPC 服务端实现（ChatService）
│   ├── StatusGrpcClient.h  # StatusService gRPC 客户端
│   ├── message.pb.h        # Protobuf 生成（勿手动修改）
│   └── message.grpc.pb.h   # gRPC 生成（勿手动修改）
├── src/                    # 所有源文件
│   ├── main.cpp            # 程序入口
│   ├── AsioIOServicePool.cpp
│   ├── CServer.cpp
│   ├── CSession.cpp
│   ├── LogicSystem.cpp
│   ├── UserMgr.cpp
│   ├── ConfigMgr.cpp
│   ├── RedisMgr.cpp
│   ├── DistLock.cpp
│   ├── MysqlDao.cpp
│   ├── MysqlMgr.cpp
│   ├── MsgNode.cpp
│   ├── ChatGrpcClient.cpp
│   ├── ChatServiceImpl.cpp
│   ├── StatusGrpcClient.cpp
│   ├── message.pb.cc       # Protobuf 生成（勿手动修改）
│   └── message.grpc.pb.cc  # gRPC 生成（勿手动修改）
└── proto/
    └── message.proto       # Protobuf 服务定义
```

## 架构概览

```
客户端 TCP
    │
    ▼
CServer（Boost.Asio Acceptor）
    │  async_accept → 分配 io_context（AsioIOServicePool 轮询）
    ▼
CSession（每连接一个）
    │  AsyncReadHead → AsyncReadBody → 投递至 LogicSystem
    ▼
LogicSystem（单线程消息队列）
    │  msg_id → FunCallBack 分发
    ├─ LoginHandler       → Redis 验证 Token，MySQL 加载用户/好友信息
    ├─ SearchInfo         → Redis / MySQL 查询用户
    ├─ AddFriendApply     → MySQL 写申请记录，同服务器直推 / gRPC 跨服务器推送
    ├─ AuthFriendApply    → MySQL 更新好友关系，同服务器直推 / gRPC 跨服务器推送
    ├─ DealChatTextMsg    → 同服务器直推 / gRPC 跨服务器推送
    └─ HeartBeatHandler   → 更新心跳时间戳，回复 ACK

ChatGrpcClient ──gRPC──▶ 对端 ChatServer（ChatServiceImpl）
                              │
                              └─ 查找 UserMgr → CSession::Send
```

### 关键组件

| 组件 | 说明 |
|------|------|
| `AsioIOServicePool` | 多个 `io_context` 线程池，round-robin 分配，提升并发吞吐 |
| `RedisConPool` | hiredis 连接池，含 60s 周期 PING 保活与自动重连 |
| `MySqlPool` | MySQL Connector/C++ 连接池，含 60s 周期健康检查 |
| `ChatConPool` | gRPC ChatService stub 连接池，每个对端服务器独立一组 |
| `DistLock` | Redis `SET NX EX` + Lua 原子释放实现分布式锁 |
| `Defer` | RAII 析构回调，保证资源释放（类似 Go defer）|

## 依赖环境

| 库 | 版本要求 | Linux 安装参考 |
|----|----------|----------------|
| CMake | ≥ 3.16 | `sudo apt install cmake` |
| GCC / Clang | 支持 C++17 | `sudo apt install g++` |
| Boost | ≥ 1.74 | `sudo apt install libboost-all-dev` |
| gRPC + Protobuf | ≥ 1.46 | `sudo apt install libgrpc++-dev protobuf-compiler-grpc` |
| hiredis | 任意 | `sudo apt install libhiredis-dev` |
| MySQL Connector/C++ | ≥ 8.0 | `sudo apt install libmysqlcppconn-dev` |
| jsoncpp | ≥ 1.9 | `sudo apt install libjsoncpp-dev` |
| pkg-config | — | `sudo apt install pkg-config` |

> **Windows（vcpkg）**：可通过 `vcpkg install boost grpc hiredis jsoncpp` 安装；MySQL Connector/C++ 需单独下载官方 MSI/ZIP 包并在 CMakeLists.txt 中设置路径。

## 配置文件说明（config.ini）

```ini
[GateServer]
Port = 8080          ; 网关端口（本服务器不使用，供参考）

[VarifyServer]
Host = 127.0.0.1
Port = 50051         ; 邮箱验证服务 gRPC 地址

[StatusServer]
Host = 0.0.0.0
Port = 50052         ; 状态服务 gRPC 地址

[Mysql]
Host = 127.0.0.1
Port = 3306
User = root
Passwd = yourpassword
Schema = llfc

[Redis]
Host = 127.0.0.1
Port = 6379
Passwd = yourpassword

[SelfServer]
Name   = chatserver1        ; 本服务器唯一名称（Redis 中用作 key）
Host   = 0.0.0.0
Port   = 8090               ; 客户端 TCP 监听端口
RPCPort = 50055             ; 本服务器 gRPC 监听端口

[PeerServer]
Servers = chatserver2       ; 对端服务器列表，逗号分隔

[chatserver2]
Name = chatserver2
Host = 127.0.0.1
Port = 50056                ; 对端服务器 gRPC 端口
```

## 构建步骤（CLion / 命令行）

```bash
# 1. 克隆并进入项目目录
cd ChatServer_2

# 2. 创建构建目录
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# 3. 编译
cmake --build build -j$(nproc)

# 4. 运行（确保 config.ini 在工作目录或构建目录中）
cd build
./ChatServer
```

### CLion 操作步骤

1. **File → Open** 选择 `ChatServer_2/` 根目录
2. CLion 自动检测 `CMakeLists.txt` 并加载项目
3. 在 **CMake** 设置面板（Settings → Build, Execution, Deployment → CMake）中：
   - Build type: `Debug` 或 `Release`
   - 若使用 vcpkg，添加 CMake 选项：`-DCMAKE_TOOLCHAIN_FILE=<vcpkg_root>/scripts/buildsystems/vcpkg.cmake`
4. 点击右上角 **Build** 或 **Run** 按钮

## 消息协议

所有 TCP 消息格式：

```
┌──────────────┬──────────────┬────────────────────────────┐
│  MsgID (2B)  │  DataLen(2B) │  JSON Body (DataLen bytes) │
└──────────────┴──────────────┴────────────────────────────┘
```

网络字节序（大端），JSON 编码的消息体。

### 主要消息 ID

| ID | 方向 | 说明 |
|----|------|------|
| 1005 | C→S | 用户登录 |
| 1006 | S→C | 登录响应（含好友列表、申请列表） |
| 1007 | C→S | 搜索用户 |
| 1008 | S→C | 搜索结果 |
| 1009 | C→S | 添加好友申请 |
| 1010 | S→C | 申请响应 |
| 1011 | S→C | 通知：收到好友申请 |
| 1013 | C→S | 同意/拒绝好友申请 |
| 1014 | S→C | 认证响应 |
| 1015 | S→C | 通知：好友认证结果 |
| 1017 | C→S | 发送文本消息 |
| 1018 | S→C | 发送响应 |
| 1019 | S→C | 通知：收到文本消息 |
| 1021 | S→C | 通知：被强制下线 |
| 1023 | C→S | 心跳包 |
| 1024 | S→C | 心跳响应 |

## gRPC 服务定义

见 [proto/message.proto](proto/message.proto)，包含三个服务：

- **VarifyService** — 邮箱验证码
- **StatusService** — 用户登录验证 & 获取聊天服务器分配
- **ChatService** — 跨聊天服务器推送（好友申请/认证/消息/踢人）

## 数据库表依赖

| 表名 | 说明 |
|------|------|
| `user` | 用户基本信息（uid, name, pwd, email, nick, sex, icon, desc） |
| `friend` | 好友关系（self_id, friend_id, back） |
| `friend_apply` | 好友申请记录（from_uid, to_uid, status） |

存储过程：`reg_user(name, email, pwd, @result)` — 注册用户并返回新 uid。
