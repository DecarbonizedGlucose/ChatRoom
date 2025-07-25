# 可执行文件：client

find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

add_executable(client
    clientmain.cpp
    TcpClient.cpp
    TopClient.cpp
    interface/winloop.cpp
    interface/output.cpp
    chat/CommManager.cpp
    chat/sqlite.cpp
)

target_link_libraries(client
    global
    io
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    protobuf::libprotobuf
    nlohmann_json::nlohmann_json
    ${CURL_LIBRARIES}
    spdlog::spdlog_header_only
    #${CURSES_LIBRARIES}
    absl_base
    absl_strings
    absl_throw_delegate
    absl_log_internal_check_op
    absl_log_internal_message
    absl_log_severity
    absl_log_globals
    absl_raw_logging_internal
    absl_log_internal_nullguard
    SQLiteCpp
    sqlite3
)

target_include_directories(client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/interface/CLI
    ${CMAKE_CURRENT_SOURCE_DIR}/../global/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../global/abstract
    ${CMAKE_CURRENT_SOURCE_DIR}/../io/include
    ${Protobuf_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${nlohmann_json_INCLUDE_DIRS}
)