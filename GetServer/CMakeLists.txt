cmake_minimum_required(VERSION 3.20)
project(GetServer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================================================================
# 用户可配置的依赖路径（在 CLion 的 CMake 设置中可修改这些缓存变量）
# ==============================================================================
set(BOOST_ROOT       "D:/cppsoft/boost_1_81_0"
    CACHE PATH "Boost 根目录")
set(GRPC_ROOT        "D:/cppsoft/grpc"
    CACHE PATH "gRPC 根目录")
set(GRPC_BUILD_DIR   "D:/cppsoft/grpc/visualpro"
    CACHE PATH "gRPC 构建输出目录（visualpro）")
set(MYSQL_ROOT       "D:/cppsoft/mysql_connector/mysql-connector-c++-8.3.0-winx64-debug/mysql-connector-c++-8.3.0-winx64"
    CACHE PATH "MySQL Connector C++ 根目录")
set(VCPKG_INSTALLED  "D:/Vcpkg/vcpkg/installed/x64-windows"
    CACHE PATH "vcpkg 安装目录")

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
# 头文件搜索路径
# ==============================================================================
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/proto
    ${BOOST_ROOT}
    ${GRPC_ROOT}/include
    ${GRPC_ROOT}/third_party/protobuf/src
    ${GRPC_ROOT}/third_party/abseil-cpp
    ${GRPC_ROOT}/third_party/address_sorting/include
    ${MYSQL_ROOT}/include
    ${VCPKG_INSTALLED}/include
)

# ==============================================================================
# 库搜索路径
# ==============================================================================
target_link_directories(${PROJECT_NAME} PRIVATE
    ${MYSQL_ROOT}/lib64/vs14/debug
    ${VCPKG_INSTALLED}/lib
    ${BOOST_ROOT}/stage/lib
    ${GRPC_BUILD_DIR}/Debug
    ${GRPC_BUILD_DIR}/third_party/cares/cares/lib/Debug
    ${GRPC_BUILD_DIR}/third_party/zlib/Debug
    ${GRPC_BUILD_DIR}/third_party/protobuf/Debug
    ${GRPC_BUILD_DIR}/third_party/boringssl-with-bazel/Debug
    ${GRPC_BUILD_DIR}/third_party/re2/Debug
    ${GRPC_BUILD_DIR}/third_party/abseil-cpp/absl/base/Debug
    ${GRPC_BUILD_DIR}/third_party/abseil-cpp/absl/strings/Debug
    ${GRPC_BUILD_DIR}/third_party/abseil-cpp/absl/time/Debug
    ${GRPC_BUILD_DIR}/third_party/abseil-cpp/absl/numeric/Debug
    ${GRPC_BUILD_DIR}/third_party/abseil-cpp/absl/hash/Debug
    ${GRPC_BUILD_DIR}/third_party/abseil-cpp/absl/container/Debug
    ${GRPC_BUILD_DIR}/third_party/abseil-cpp/absl/debugging/Debug
    ${GRPC_BUILD_DIR}/third_party/abseil-cpp/absl/flags/Debug
    ${GRPC_BUILD_DIR}/third_party/abseil-cpp/absl/random/Debug
    ${GRPC_BUILD_DIR}/third_party/abseil-cpp/absl/status/Debug
    ${GRPC_BUILD_DIR}/third_party/abseil-cpp/absl/synchronization/Debug
    ${GRPC_BUILD_DIR}/third_party/abseil-cpp/absl/types/Debug
)

# ==============================================================================
# 链接库
# ==============================================================================
target_link_libraries(${PROJECT_NAME} PRIVATE
    # gRPC & Protobuf
    libprotobufd
    gpr
    grpc
    grpc++
    grpc++_reflection
    address_sorting
    upb
    cares
    zlibstaticd
    # OpenSSL (BoringSSL)
    ssl
    crypto
    # Abseil
    absl_bad_any_cast_impl
    absl_bad_optional_access
    absl_bad_variant_access
    absl_base
    absl_city
    absl_civil_time
    absl_cord
    absl_debugging_internal
    absl_demangle_internal
    absl_examine_stack
    absl_exponential_biased
    absl_failure_signal_handler
    absl_flags
    absl_flags_config
    absl_flags_internal
    absl_flags_marshalling
    absl_flags_parse
    absl_flags_program_name
    absl_flags_usage
    absl_flags_usage_internal
    absl_graphcycles_internal
    absl_hash
    absl_hashtablez_sampler
    absl_int128
    absl_leak_check
    absl_leak_check_disable
    absl_log_severity
    absl_malloc_internal
    absl_periodic_sampler
    absl_random_distributions
    absl_random_internal_distribution_test_util
    absl_random_internal_pool_urbg
    absl_random_internal_randen
    absl_random_internal_randen_hwaes
    absl_random_internal_randen_hwaes_impl
    absl_random_internal_randen_slow
    absl_random_internal_seed_material
    absl_random_seed_gen_exception
    absl_random_seed_sequences
    absl_raw_hash_set
    absl_raw_logging_internal
    absl_scoped_set_env
    absl_spinlock_wait
    absl_stacktrace
    absl_status
    absl_statusor
    absl_strings
    absl_strings_internal
    absl_str_format_internal
    absl_symbolize
    absl_synchronization
    absl_throw_delegate
    absl_time
    absl_time_zone
    # RE2
    re2
    # MySQL Connector C++
    mysqlcppconn
    mysqlcppconn8
    # Windows
    ws2_32
)

# ==============================================================================
# 构建后：将 config.ini 复制到输出目录
# ==============================================================================
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/config.ini
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/config.ini
    COMMENT "Copying config.ini to output directory"
)
