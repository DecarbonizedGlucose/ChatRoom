# 静态库：io
add_library(io STATIC
    net/Socket.cpp
    net/reactor.cpp
    net/ioaction.cpp
    
)

target_link_libraries(io PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

target_include_directories(io PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)