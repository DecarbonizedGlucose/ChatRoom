#ifndef TCPSERVER_HPP
#define TCPSERVER_HPP

#include "../../io/include/reactor.hpp"
#include "../../global/include/threadpool.hpp"
#include "../../global/include/command.hpp"
#include "connection_manager.hpp"
#include <functional>
#include "dispatcher.hpp"
#include <unordered_map>
#include "TcpServerConnection.hpp"

class TcpServer {
private:
    std::shared_ptr<reactor> pr = nullptr;              // 反应堆, 事件丢到这里
    std::shared_ptr<ListenSocket> listen_conn = nullptr; // 监听套接字(属于event,这里仅仅拉出来)
    std::shared_ptr<thread_pool> pool = nullptr;    // (外援)线程池, 事件回调丢这里
    bool running = false;

public:
    // 连接池进程内放着，用户在线状态存到redis中
    std::unordered_map<std::string, TcpServerConnection*> user_connections; // 用户连接池, user_ID -> TcpServerConnection
    Dispatcher* disp = nullptr; // 分发器, 事件分发到这里
    int idx;

    TcpServer(int idx);
    TcpServer(int idx, const std::string& ip, uint16_t port);
    ~TcpServer();

    // 获取fd
    int get_lfd() const;
    int get_efd() const;

    // 配置和初始化
    void init(std::shared_ptr<thread_pool> pool, RedisController* re, Dispatcher* disp);
    void start();
    void stop();
    void auto_accept();
    void heartbeat_monitor_loop(int interval_sec = 60, int timeout_sec = 90); // 心跳监控
};

#endif