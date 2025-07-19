#include "include/TopClient.hpp"
#include "include/TcpClient.hpp"
#include "../global/include/threadpool.hpp"
#include "include/CLI/TerminalInput.hpp"
#include "include/CLI/winloop.hpp"
#include "include/chat/main/CommManager.hpp"

TopClient::TopClient() {
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
    comm_manager = new CommManager(this);
    input = new TerminalInput();
    start_win = new StartWin(input, comm_manager);
    main_win = new MainWin(input, comm_manager);
}

void TopClient::launch() {
    // 启动三个客户端
    message_client->start();
    command_client->start();
    data_client->start();
    // 初始化线程池
    pool->init();

    while (running) {
        start_win->init();
        start_win->main_loop();
        main_win->init();
        main_win->main_loop();
    }
}

void TopClient::stop() {
    // 停止三个客户端
    message_client->stop();
    command_client->stop();
    data_client->stop();
    // 停止线程池
    pool->stop();
    running = false;
}