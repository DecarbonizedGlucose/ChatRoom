# 可执行文件：server
add_executable(server
    servermain.cpp
    TcpServer.cpp
    TopServer.cpp
    TcpServerConnection.cpp
    chat/email.cpp
    connection_manager.cpp
    chat/dispatcher.cpp
    chat/handler.cpp
    chat/sfile_manager.cpp
    database/redis.cpp
    database/mysql.cpp
)

target_link_libraries(server
    global
    io
    ${CURL_LIBRARIES}
    ${MYSQL_LIBRARIES}
    redis++::redis++
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    protobuf::libprotobuf
    nlohmann_json::nlohmann_json
    spdlog::spdlog_header_only
)

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
    ${MYSQL_INCLUDE_DIRS}
    ${REDIS++_INCLUDE_DIRS}
    ${HIREDIS_INCLUDE_DIRS}
    ${nlohmann_json_INCLUDE_DIRS}
)