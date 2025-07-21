cmake_minimum_required(VERSION 3.10)
project(ChatRoom)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 输出目录设置（推荐）
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)

# 查找外部库
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)
find_package(redis++ REQUIRED)
find_package(PkgConfig REQUIRED)

# MariaDB / MySQL 兼容处理
pkg_check_modules(MARIADB mariadb)

if (NOT MARIADB_FOUND)
    message(STATUS "MariaDB not found, trying MySQL...")

    find_path(MYSQL_INCLUDE_DIR mysql/mysql.h)
    find_library(MYSQL_LIBRARY NAMES mysqlclient)

    if (MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)
        message(STATUS "MySQL found")
        set(MARIADB_INCLUDE_DIRS ${MYSQL_INCLUDE_DIR})
        set(MARIADB_LIBRARIES ${MYSQL_LIBRARY})
    else()
        message(FATAL_ERROR "Neither MariaDB nor MySQL was found.")
    endif()
else()
    set(MARIADB_LIBRARIES ${MARIADB_LIBRARIES})
endif()

find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# 包含目录
include_directories(
    ${PROJECT_SOURCE_DIR}/project/global/abstract
    ${PROJECT_SOURCE_DIR}/project/global/include
    ${PROJECT_SOURCE_DIR}/project/io/include
    ${PROJECT_SOURCE_DIR}/project/server/include
    ${PROJECT_SOURCE_DIR}/project/client/include
    ${PROJECT_SOURCE_DIR}/project/client/chat
    ${Protobuf_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${MARIADB_INCLUDE_DIRS}
    ${REDIS++_INCLUDE_DIRS}
    ${HIREDIS_INCLUDE_DIRS}
    ${nlohmann_json_INCLUDE_DIRS}
)

add_subdirectory(project/global)
add_subdirectory(project/io)
add_subdirectory(project/server)
add_subdirectory(project/client)

set(CMAKE_BUILD_TYPE Debug)

# 关闭优化，确保加上调试信息
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_C_FLAGS_DEBUG "-g -O0")
