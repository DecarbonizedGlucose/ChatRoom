#include "include/TopClient.hpp"
#include "include/TcpClient.hpp"
#include "../global/include/threadpool.hpp"
#include "include/CommManager.hpp"
#include "include/winloop.hpp"

TopClient::TopClient() {
    message_client = new TcpClient(
        set_addr_c::client_addr[0].first,
        set_addr_c::client_addr[0].second);
    command_client = new TcpClient(
        set_addr_c::client_addr[1].first,
        set_addr_c::client_addr[1].second);
    data_client = new TcpClient(
        set_addr_c::client_addr[2].first,
        set_addr_c::client_addr[2].second);
    pool = new thread_pool(8);
    comm = new CommManager(this);
    winloop = new WinLoop(comm, pool);
    comm->win = winloop;
}

TopClient::~TopClient() {
    delete message_client;
    delete command_client;
    delete data_client;
    delete pool;
    delete winloop;
}

void TopClient::launch() {
    running = true;
    // 启动三个客户端
    message_client->start();
    command_client->start();
    data_client->start();
    // 初始化线程池
    pool->init();
    // 启动命令行界面
    winloop->run();
    stop();
    return;
}

void TopClient::stop() {
    // 停止三个客户端
    message_client->stop();
    command_client->stop();
    data_client->stop();
    // 停止线程池
    pool->shutdown();
    winloop->stop();
    running = false;
}