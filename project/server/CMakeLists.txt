# 可执行文件：server
add_executable(server
    servermain.cpp
    TcpServer.cpp
    TopServer.cpp
    chat/email.cpp
    connection_manager.cpp
    chat/dispatcher.cpp
    chat/handler.cpp
    database/redis.cpp
    database/mysql.cpp
)

target_link_libraries(server
    global
    io
    ${CURL_LIBRARIES}
    ${MARIADB_LINK_LIBRARIES}
    redis++::redis++
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    protobuf::libprotobuf
    nlohmann_json::nlohmann_json
    absl_base
    absl_strings
    absl_throw_delegate
    absl_log_internal_check_op
    absl_log_internal_message
    absl_log_severity
    absl_log_globals
    absl_raw_logging_internal
    absl_log_internal_nullguard
    spdlog::spdlog_header_only
)

# 添加链接目录
target_link_directories(server PRIVATE ${MARIADB_LIBRARY_DIRS})

target_include_directories(server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../global/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../global/abstract
    ${CMAKE_CURRENT_SOURCE_DIR}/../io/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../client/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../client/chat
    ${Protobuf_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${MARIADB_INCLUDE_DIRS}
    ${REDIS++_INCLUDE_DIRS}
    ${HIREDIS_INCLUDE_DIRS}
    ${nlohmann_json_INCLUDE_DIRS}
)