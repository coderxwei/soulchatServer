cmake_minimum_required(VERSION 3.21)
project(ChatServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# vcpkg 工具链路径 (传入方式: -DCMAKE_TOOLCHAIN_FILE=... 或写在此处)
# ---------------------------------------------------------------------------
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "D:/Vcpkg/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain" FORCE)
endif()

# ---------------------------------------------------------------------------
# 路径配置
# ---------------------------------------------------------------------------
set(MYSQL_ROOT "D:/cppsoft/mysql_connector/mysql-connector-c++-8.3.0-winx64-debug/mysql-connector-c++-8.3.0-winx64")

# ---------------------------------------------------------------------------
# 通过 vcpkg cmake 集成查找依赖包
# ---------------------------------------------------------------------------
find_package(gRPC CONFIG REQUIRED)
find_package(protobuf CONFIG REQUIRED)

# hiredis 可能不支持 CONFIG 模式，先尝试 MODULE 模式，失败则手动查找
find_package(hiredis QUIET)
if(NOT hiredis_FOUND)
    # 手动查找 hiredis（使用 vcpkg 安装路径）
    # vcpkg 工具链会设置 VCPKG_INSTALLED_DIR 变量
    if(DEFINED VCPKG_INSTALLED_DIR)
        set(VCPKG_HIREDIS_ROOT "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    else()
        # 回退到硬编码路径
        set(VCPKG_HIREDIS_ROOT "D:/Vcpkg/vcpkg/installed/x64-windows")
    endif()
    
    find_path(HIREDIS_INCLUDE_DIR 
        NAMES hiredis/hiredis.h
        PATHS "${VCPKG_HIREDIS_ROOT}/include"
    )
    find_library(HIREDIS_LIBRARY
        NAMES hiredis
        PATHS "${VCPKG_HIREDIS_ROOT}/lib"
    )
    
    if(HIREDIS_INCLUDE_DIR AND HIREDIS_LIBRARY)
        message(STATUS "Found hiredis manually: ${HIREDIS_INCLUDE_DIR}")
        add_library(hiredis::hiredis INTERFACE IMPORTED)
        set_target_properties(hiredis::hiredis PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${HIREDIS_INCLUDE_DIR}"
            INTERFACE_LINK_LIBRARIES "${HIREDIS_LIBRARY}"
        )
    else()
        message(FATAL_ERROR "Could not find hiredis. Please install it via vcpkg: vcpkg install hiredis")
    endif()
endif()

find_package(jsoncpp CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Boost REQUIRED COMPONENTS filesystem system)

# ---------------------------------------------------------------------------
# 源文件 (位于 src/ 目录)
# ---------------------------------------------------------------------------
set(SOURCES
    src/AsioIOServicePool.cpp
    src/ChatGrpcClient.cpp
    src/ChatServer.cpp
    src/ChatServiceImpl.cpp
    src/ConfigMgr.cpp
    src/CServer.cpp
    src/CSession.cpp
    src/DistLock.cpp
    src/LogicSystem.cpp
    src/message.grpc.pb.cc
    src/message.pb.cc
    src/MsgNode.cpp
    src/MysqlDao.cpp
    src/MysqlMgr.cpp
    src/RedisMgr.cpp
    src/StatusGrpcClient.cpp
    src/UserMgr.cpp
)

add_executable(ChatServer ${SOURCES})

# ---------------------------------------------------------------------------
# MSVC 编译选项
# ---------------------------------------------------------------------------
if(MSVC)
    target_compile_options(ChatServer PRIVATE
        /W3          # 警告级别 3
        /permissive- # 一致性模式
        /sdl         # SDL 安全检查
        $<$<CONFIG:Release>:/Gy>
        $<$<CONFIG:Release>:/Oi>
    )
endif()

# ---------------------------------------------------------------------------
# 预处理器定义
# ---------------------------------------------------------------------------
target_compile_definitions(ChatServer PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x0A00
    _CONSOLE
    _CRT_SECURE_NO_WARNINGS
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# ---------------------------------------------------------------------------
# 头文件包含目录
# ---------------------------------------------------------------------------
target_include_directories(ChatServer PRIVATE
    include                     # 所有头文件均在 include/ 下
    "${MYSQL_ROOT}/include"
)

# ---------------------------------------------------------------------------
# 链接库 (通过 cmake targets, vcpkg 自动处理 Debug/Release)
# ---------------------------------------------------------------------------
target_link_libraries(ChatServer PRIVATE
    # gRPC
    gRPC::grpc++
    gRPC::grpc++_reflection

    # Protobuf
    protobuf::libprotobuf

    # hiredis
    hiredis::hiredis

    # jsoncpp
    JsonCpp::JsonCpp

    # ZLIB
    ZLIB::ZLIB

    # Boost.Filesystem + Boost.System
    Boost::filesystem
    Boost::system

    # MySQL Connector C++ (绝对路径，仅有 debug 版本)
    "${MYSQL_ROOT}/lib64/vs14/debug/mysqlcppconn.lib"
    "${MYSQL_ROOT}/lib64/vs14/debug/mysqlcppconn8.lib"

    # Windows 系统库
    ws2_32
    Crypt32
    Secur32
)

# ---------------------------------------------------------------------------
# 构建后事件：复制 config.ini 和 DLL 到输出目录
# ---------------------------------------------------------------------------
add_custom_command(TARGET ChatServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/config.ini"
        "$<TARGET_FILE_DIR:ChatServer>/config.ini"
    COMMENT "Copying config.ini"
)

file(GLOB PROJECT_DLLS "${CMAKE_SOURCE_DIR}/*.dll")
foreach(DLL_FILE ${PROJECT_DLLS})
    add_custom_command(TARGET ChatServer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DLL_FILE}"
            "$<TARGET_FILE_DIR:ChatServer>"
    )
endforeach()
