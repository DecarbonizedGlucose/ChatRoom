# 静态库：global
add_library(global STATIC
    abstract/command.pb.cc
    abstract/data.pb.cc
    abstract/envelope.pb.cc
    abstract/message.pb.cc
    entity/file.cpp
    abstract/datatypes.cpp
)

target_link_libraries(global PUBLIC
    protobuf::libprotobuf
    nlohmann_json::nlohmann_json
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_include_directories(global PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/abstract
)