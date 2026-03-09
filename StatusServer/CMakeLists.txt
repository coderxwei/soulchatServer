cmake_minimum_required(VERSION 3.20)
project(StatusServer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows 目标版本：Windows 10
add_compile_definitions(_WIN32_WINNT=0x0A00)

# MSVC：源文件和诊断输出统一使用 UTF-8，消除乱码和 C4819 警告
if(MSVC)
    add_compile_options(/utf-8)
endif()

# ============================================================
# vcpkg 路径（所有第三方库均通过 vcpkg 管理）
# ============================================================
set(VCPKG_ROOT "D:/Vcpkg/vcpkg/installed/x64-windows"
        CACHE PATH "vcpkg installed root")

# ============================================================
# 头文件搜索路径
# ============================================================
include_directories(
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/proto
        ${VCPKG_ROOT}/include
)

# ============================================================
# 库文件搜索路径（Debug/Release 自动切换）
# ============================================================
link_directories(
        $<IF:$<CONFIG:Debug>,${VCPKG_ROOT}/debug/lib,${VCPKG_ROOT}/lib>
)

# ============================================================
# 源文件
# ============================================================
file(GLOB_RECURSE SOURCES
        "src/*.cpp"
        "proto/*.cc"
)

add_executable(${PROJECT_NAME} ${SOURCES})

# ============================================================
# 链接库
# ============================================================
target_link_libraries(${PROJECT_NAME}
        # gRPC
        grpc++
        grpc++_reflection
        grpc
        gpr
        address_sorting

        # Protobuf
        $<IF:$<CONFIG:Debug>,libprotobufd,libprotobuf>

        # upb
        upb_base_lib
        upb_json_lib
        upb_mem_lib
        upb_message_lib
        upb_mini_descriptor_lib
        upb_textformat_lib
        upb_wire_lib

        # TLS (OpenSSL)
        libssl
        libcrypto

        # 其他依赖
        cares
        re2
        $<IF:$<CONFIG:Debug>,zlibd,zlib>

        # Abseil（vcpkg 动态库，通过 abseil_dll.lib 导入）
        absl_flags_commandlineflag
        absl_flags_commandlineflag_internal
        absl_flags_config
        absl_flags_internal
        absl_flags_marshalling
        absl_flags_parse
        absl_flags_private_handle_accessor
        absl_flags_program_name
        absl_flags_reflection
        absl_flags_usage
        absl_flags_usage_internal
        absl_log_flags

        # hiredis
        $<IF:$<CONFIG:Debug>,hiredisd,hiredis>

        # Windows
        ws2_32
)

# ============================================================
# 构建后：拷贝 config.ini 和运行时 DLL
# ============================================================
set(VCPKG_BIN $<IF:$<CONFIG:Debug>,${VCPKG_ROOT}/debug/bin,${VCPKG_ROOT}/bin>)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/config.ini
                $<TARGET_FILE_DIR:${PROJECT_NAME}>/config.ini
        # 将 vcpkg 运行时 DLL 拷贝到输出目录
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${VCPKG_BIN}
                $<TARGET_FILE_DIR:${PROJECT_NAME}>
)
