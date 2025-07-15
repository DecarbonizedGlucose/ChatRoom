#ifndef TOP_SERVER_HPP
#define TOP_SERVER_HPP

#include <iostream>
#include "TcpServer.hpp"
#include "dispatcher.hpp"
#include "../../global/include/threadpool.hpp"

class TopServer {
public:
    std::shared_ptr<thread_pool> pool = nullptr; // 线程池
    TcpServer* message_server = nullptr; // 消息服务器
    TcpServer* command_server = nullptr; // 命令服务器
    TcpServer* data_server = nullptr; // 数据服务器
    Dispatcher* disp = nullptr; // 分发器
    RedisController* redis = nullptr; // Redis控制器

    TopServer() {
        pool = std::make_shared<thread_pool>();
        redis = new RedisController();
        disp = new Dispatcher(redis);
        message_server = new TcpServer(0);
        command_server = new TcpServer(1);
        data_server = new TcpServer(2);
        disp->add_server(message_server, 0);
        disp->add_server(command_server, 1);
        disp->add_server(data_server, 2);
    }

    ~TopServer() {
        delete message_server;
        delete command_server;
        delete data_server;
        delete disp;
        delete redis;
        pool.reset();
    }

    void launch() {
        pool->init();
        message_server->init(pool, redis, disp);
        command_server->init(pool, redis, disp);
        data_server->init(pool, redis, disp);
        pool->submit([this]() {
            message_server->start();
        });
        pool->submit([this]() {
            command_server->start();
        });
        pool->submit([this]() {
            data_server->start();
        });
    }
};



#endif // TOP_SERVER_HPP