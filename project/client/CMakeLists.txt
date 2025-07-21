# 可执行文件：client
add_executable(client
    clientmain.cpp
    TcpClient.cpp
    TopClient.cpp
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