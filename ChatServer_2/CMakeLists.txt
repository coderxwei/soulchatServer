cmake_minimum_required(VERSION 3.16)
project(ChatServer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# 依赖库路径配置（根据实际安装路径修改）
# ============================================================

# Boost
find_package(Boost 1.74 REQUIRED COMPONENTS
    system
    filesystem
    thread
)

# Protobuf
find_package(Protobuf REQUIRED)

# gRPC（使用 cmake 配置文件，推荐通过 vcpkg 或 apt 安装）
find_package(gRPC CONFIG REQUIRED)

# jsoncpp
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

# hiredis（Redis C 客户端）
pkg_check_modules(HIREDIS REQUIRED hiredis)

# MySQL Connector/C++ (jdbc)
# 若通过系统包管理器安装，路径通常如下；否则手动设置
find_path(MYSQL_CONNECTOR_INCLUDE_DIR
    NAMES jdbc/mysql_driver.h
    PATHS
        /usr/include
        /usr/local/include
        /opt/mysql-connector-c++/include
)
find_library(MYSQL_CONNECTOR_LIB
    NAMES mysqlcppconn
    PATHS
        /usr/lib
        /usr/local/lib
        /opt/mysql-connector-c++/lib64
        /opt/mysql-connector-c++/lib
)

if(NOT MYSQL_CONNECTOR_INCLUDE_DIR OR NOT MYSQL_CONNECTOR_LIB)
    message(FATAL_ERROR
        "MySQL Connector/C++ not found.\n"
        "Install it via: sudo apt install libmysqlcppconn-dev\n"
        "Or set MYSQL_CONNECTOR_INCLUDE_DIR and MYSQL_CONNECTOR_LIB manually."
    )
endif()

message(STATUS "MySQL Connector include: ${MYSQL_CONNECTOR_INCLUDE_DIR}")
message(STATUS "MySQL Connector lib:     ${MYSQL_CONNECTOR_LIB}")

# ============================================================
# 源文件列表
# ============================================================
set(SOURCES
    src/main.cpp
    src/AsioIOServicePool.cpp
    src/ChatGrpcClient.cpp
    src/ChatServiceImpl.cpp
    src/ConfigMgr.cpp
    src/CServer.cpp
    src/CSession.cpp
    src/DistLock.cpp
    src/LogicSystem.cpp
    src/MsgNode.cpp
    src/MysqlDao.cpp
    src/MysqlMgr.cpp
    src/RedisMgr.cpp
    src/StatusGrpcClient.cpp
    src/UserMgr.cpp
    # Protobuf / gRPC 生成代码
    src/message.pb.cc
    src/message.grpc.pb.cc
)

# ============================================================
# 可执行目标
# ============================================================
add_executable(${PROJECT_NAME} ${SOURCES})

# ============================================================
# 头文件搜索路径
# ============================================================
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
    ${Protobuf_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
    ${HIREDIS_INCLUDE_DIRS}
    ${MYSQL_CONNECTOR_INCLUDE_DIR}
)

# ============================================================
# 链接库
# ============================================================
target_link_libraries(${PROJECT_NAME} PRIVATE
    # Boost
    Boost::system
    Boost::filesystem
    Boost::thread
    # gRPC + Protobuf
    gRPC::grpc++
    gRPC::grpc
    protobuf::libprotobuf
    # JSON
    ${JSONCPP_LIBRARIES}
    # Redis
    ${HIREDIS_LIBRARIES}
    # MySQL Connector/C++
    ${MYSQL_CONNECTOR_LIB}
    # 线程支持
    pthread
)

# ============================================================
# 编译选项
# ============================================================
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
)

# ============================================================
# 将 config.ini 复制到构建目录（运行时需要）
# ============================================================
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.ini
    ${CMAKE_CURRENT_BINARY_DIR}/config.ini
    COPYONLY
)
