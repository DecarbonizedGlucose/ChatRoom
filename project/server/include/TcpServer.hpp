#pragma once

#include "../../global/include/threadpool.hpp"
#include "../../global/include/command.hpp"
#include <functional>
#include "TcpServerConnection.hpp"

class Dispatcher;
class reactor;
class RedisController;
class ListenSocket;

namespace set_addr_s {
    using Addr = std::pair<std::string, uint16_t>;
    extern Addr server_addr[3];

    bool fetch_addr_from_config();
}

class TcpServer {
private:
    reactor* pr = nullptr; // 反应堆, 事件丢到这里
    ListenSocket* listen_conn = nullptr; // 监听套接字
    thread_pool* pool = nullptr; // (外援)线程池, 事件回调丢这里
    bool running = false;

public:

    Dispatcher* disp = nullptr; // 分发器, 事件分发到这里
    int idx;

    TcpServer(int idx);
    TcpServer(int idx, const std::string& ip, uint16_t port);
    ~TcpServer();

    // 获取fd
    int get_lfd() const;
    int get_efd() const;

    // 配置和初始化
    void init(thread_pool* pool, RedisController* re, Dispatcher* disp);
    void start();
    void stop();
    void auto_accept();
    void heartbeat_monitor_loop(int interval_sec = 60, int timeout_sec = 90); // 心跳监控
};
