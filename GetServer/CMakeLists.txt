cmake_minimum_required(VERSION 3.20)

# ==============================================================================
# vcpkg 工具链（必须在 project() 之前设置）
# CLion 中也可在 Settings → Build → CMake → CMake options 里设置：
#   -DCMAKE_TOOLCHAIN_FILE=D:/Vcpkg/vcpkg/scripts/buildsystems/vcpkg.cmake
# ==============================================================================
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "D:/Vcpkg/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain file")
endif()
set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "vcpkg triplet")

project(GetServer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================================================================
# 查找 vcpkg 提供的依赖包
# ==============================================================================
find_package(gRPC CONFIG REQUIRED)
find_package(protobuf CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS filesystem system)
find_package(jsoncpp CONFIG REQUIRED)
find_package(hiredis CONFIG REQUIRED)

# ==============================================================================
# MySQL Connector C++（独立安装，非 vcpkg）
# ==============================================================================
set(MYSQL_ROOT
    "D:/cppsoft/mysql_connector/mysql-connector-c++-8.3.0-winx64-debug/mysql-connector-c++-8.3.0-winx64"
    CACHE PATH "MySQL Connector C++ 根目录")

# ==============================================================================
# 源文件
# ==============================================================================
set(SOURCES
    src/AsioIOServicePool.cpp
    src/CServer.cpp
    src/ConfigMgr.cpp
    src/GateServer.cpp
    src/HttpConnection.cpp
    src/LogicSystem.cpp
    src/MysqlDao.cpp
    src/MysqlMgr.cpp
    src/RedisMgr.cpp
    src/StatusGrpcClient.cpp
    src/VerifyGrpcClient.cpp
    proto/message.pb.cc
    proto/message.grpc.pb.cc
)

add_executable(${PROJECT_NAME} ${SOURCES})

# ==============================================================================
# 编译宏定义
# ==============================================================================
target_compile_definitions(${PROJECT_NAME} PRIVATE
    # 目标 Windows 10+（Boost.Asio 必须明确指定，否则报警告/错误）
    _WIN32_WINNT=0x0A00
    WIN32_LEAN_AND_MEAN   # 减少 Windows.h 包含范围，避免宏冲突
)

# MSVC：源文件和输出均使用 UTF-8，避免中文乱码
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

# ==============================================================================
# 头文件搜索路径
# ==============================================================================
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/proto
    ${MYSQL_ROOT}/include
)

# ==============================================================================
# 链接库（vcpkg 的 imported targets 自动处理 include 和 lib 路径）
# ==============================================================================
target_link_libraries(${PROJECT_NAME} PRIVATE
    # gRPC（自动携带 protobuf、abseil、ssl、cares、zlib 等所有传递依赖）
    gRPC::grpc++
    gRPC::grpc++_reflection
    # Protobuf
    protobuf::libprotobuf
    # Boost
    Boost::filesystem
    Boost::system
    # JSON
    JsonCpp::JsonCpp
    # Redis
    hiredis::hiredis
    # MySQL Connector C++
    ${MYSQL_ROOT}/lib64/vs14/$<IF:$<CONFIG:Debug>,debug,release>/mysqlcppconn.lib
    ${MYSQL_ROOT}/lib64/vs14/$<IF:$<CONFIG:Debug>,debug,release>/mysqlcppconn8.lib
    # Windows
    ws2_32
)

# ==============================================================================
# 构建后：复制运行时依赖到输出目录
# ==============================================================================

# config.ini
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/config.ini
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/config.ini
    COMMENT "Copying config.ini"
)

# MySQL Connector C++ 运行时 DLL（不在 vcpkg 中，需手动复制）
set(MYSQL_DLL_DIR ${MYSQL_ROOT}/lib64/$<IF:$<CONFIG:Debug>,debug,release>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${MYSQL_DLL_DIR}/mysqlcppconn-9-vs14.dll
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/mysqlcppconn-9-vs14.dll
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${MYSQL_DLL_DIR}/mysqlcppconn8-2-vs14.dll
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/mysqlcppconn8-2-vs14.dll
    COMMENT "Copying MySQL Connector C++ runtime DLLs"
)
