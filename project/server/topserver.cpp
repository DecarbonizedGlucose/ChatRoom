#include "include/topserver.hpp"

TopServer::TopServer(std::shared_ptr<thread_pool> pool) {
    this->pool = pool;
    command_server = std::make_shared<TS>();
    data_server = std::make_shared<TS>();
    command_server->set_thread_pool(pool);
    data_server->set_thread_pool(pool);
}

TopServer::~TopServer() {
    command_server.reset();
    data_server.reset();
    pool.reset();
}

void TopServer::launch() {
    // 现在改成3通道了，通信格式也改变了。必须重写
    // if (!command_server->listen_init()) {
    //     throw std::runtime_error("Failed to initialize command server");
    // }
    // if (!data_server->listen_init()) {
    //     throw std::runtime_error("Failed to initialize data server");
    // }
    // pool->submit([this]() {
    //     command_server->launch();
    // });
    // pool->submit([this]() {
    //     data_server->launch();
    // });
}