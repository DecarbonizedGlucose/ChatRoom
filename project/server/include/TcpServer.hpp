#ifndef TCPSERV_HPP
#define TCPSERV_HPP

#include "../../io/include/reactor.hpp"
#include "../../global/include/threadpool.hpp"
#include <functional>

class TcpServer {
private:
    std::shared_ptr<rea> pr = nullptr;              // 反应堆, 事件丢到这里
    std::shared_ptr<LSocket> listen_conn = nullptr; // 监听套接字
    std::shared_ptr<thread_pool> pool = nullptr;    // (外援)线程池, 事件回调丢这里
    bool running = false;

public:
    TcpServer();
    TcpServer(const std::string& ip, uint16_t port, sa_family_t family = AF_INET);
    ~TcpServer();

    int get_lfd() const;
    int get_efd() const;

    void set_thread_pool(std::shared_ptr<thread_pool> pool);
    bool listen_init();
    void accept_connections(std::function<void()> cb = nullptr);
    void launch();
    void stop();
};

using TS = TcpServer;

#endif