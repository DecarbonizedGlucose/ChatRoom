#include "include/TopServer.hpp"
#include "include/TcpServer.hpp"
#include "include/dispatcher.hpp"
#include "../global/include/threadpool.hpp"
#include "../global/include/logging.hpp"

TopServer::TopServer() {
    pool = std::make_unique<thread_pool>(20, 30);
    redis = new RedisController();
    disp = new Dispatcher(redis);
    message_server = new TcpServer(0);
    command_server = new TcpServer(1);
    data_server = new TcpServer(2);
    disp->add_server(message_server, 0);
    disp->add_server(command_server, 1);
    disp->add_server(data_server, 2);
}

TopServer::~TopServer() {
    log_info("TopServer destructor called, cleaning up resources");
    delete message_server;
    delete command_server;
    delete data_server;
    delete disp;
    delete redis;
    pool.reset();
}

void TopServer::launch() {
    pool->init();
    message_server->init(pool.get(), redis, disp);
    command_server->init(pool.get(), redis, disp);
    data_server->init(pool.get(), redis, disp);
    pool->submit([this]() {
        message_server->start();
    });
    log_info("Message server submited to thread pool");
    pool->submit([this]() {
        command_server->start();
    });
    log_info("Command server submited to thread pool");
    pool->submit([this]() {
        data_server->start();
    });
    log_info("Data server submited to thread pool");
}

void TopServer::stop() {
    log_info("Stopping TopServer and all associated servers");
    if (message_server) {
        message_server->stop();
    }
    if (command_server) {
        command_server->stop();
    }
    if (data_server) {
        data_server->stop();
    }
    pool->shutdown();
    log_info("All servers stopped");
}

