#pragma once

#include "../../client/include/TcpClient.hpp"
#include "../../global/include/threadpool.hpp"

class TopClient {
public:
    std::unique_ptr<TcpClient> message_client;
    std::unique_ptr<TcpClient> command_client;
    std::unique_ptr<TcpClient> data_client;
    std::unique_ptr<thread_pool> pool;
    bool running = false;

    TopClient() {
        // 这里初始化三个Client实例
        // 需要ip和port*3
        message_client = std::make_unique<TcpClient>(
            set_addr_c::client_addr[0].first,
            set_addr_c::client_addr[0].second);
        command_client = std::make_unique<TcpClient>(
            set_addr_c::client_addr[1].first,
            set_addr_c::client_addr[1].second);
        data_client = std::make_unique<TcpClient>(
            set_addr_c::client_addr[2].first,
            set_addr_c::client_addr[2].second);
        pool = std::make_unique<thread_pool>();
    }

    void launch() {
        // 启动三个客户端
        message_client->start();
        command_client->start();
        data_client->start();
        // 初始化线程池
        pool->init();

        while (running) {
            // 客户端主循环
        }
    }

    void stop() {
        // 停止三个客户端
        message_client->stop();
        command_client->stop();
        data_client->stop();
        // 停止线程池
        pool->stop();
        running = false;
    }
};
