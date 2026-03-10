cmake_minimum_required(VERSION 3.20)

# vcpkg toolchain（必须在 project() 之前）
set(CMAKE_TOOLCHAIN_FILE "D:/Vcpkg/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE PATH "vcpkg toolchain")

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
# 查找依赖（vcpkg 自动处理所有传递依赖）
# ============================================================
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(hiredis CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS filesystem)

# ============================================================
# 头文件搜索路径
# ============================================================
include_directories(
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/proto
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
# 链接库（通过 CMake targets，传递依赖自动处理）
# ============================================================
target_link_libraries(${PROJECT_NAME}
        gRPC::grpc++
        gRPC::grpc++_reflection
        protobuf::libprotobuf
        hiredis::hiredis
        Boost::filesystem
        ws2_32
)

# ============================================================
# 构建后：拷贝 config.ini 和运行时 DLL
# ============================================================
set(VCPKG_ROOT "D:/Vcpkg/vcpkg/installed/x64-windows")
set(VCPKG_BIN $<IF:$<CONFIG:Debug>,${VCPKG_ROOT}/debug/bin,${VCPKG_ROOT}/bin>)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/config.ini
                $<TARGET_FILE_DIR:${PROJECT_NAME}>/config.ini
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${VCPKG_BIN}
                $<TARGET_FILE_DIR:${PROJECT_NAME}>
)
