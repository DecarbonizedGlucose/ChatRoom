#ifndef TCPSERV_HPP
#define TCPSERV_HPP

#include "../../io/include/reactor.hpp"
#include "../../global/include/threadpool.hpp"
#include "../../global/include/command.hpp"
#include "connection_manager.hpp"
#include <functional>

class TcpServer {
private:
    std::shared_ptr<rea> pr = nullptr;              // 反应堆, 事件丢到这里
    std::shared_ptr<LSocket> listen_conn = nullptr; // 监听套接字
    std::shared_ptr<thread_pool> pool = nullptr;    // (外援)线程池, 事件回调丢这里
    std::shared_ptr<ConnectionManager> conn_manager = nullptr; // 连接管理器
    bool running = false;

public:
    TcpServer();
    TcpServer(const std::string& ip, uint16_t port, sa_family_t family = AF_INET);
    ~TcpServer();

    // 获取fd
    int get_lfd() const;
    int get_efd() const;

    // 配置和初始化
    void set_thread_pool(std::shared_ptr<thread_pool> pool);
    bool listen_init(void (*first_func)(event<>*) = nullptr); // 这写的啥啊？
    void accept_connections(std::function<std::any()> cb);
    void launch();
    void stop();

    // // 核心功能：中转
    // // 由一个用户发送的消息或命令，转发给另一个用户/另一群组
    // void transfer_content(const std::string& user_ID, const MesPtr& message); // 内部区分消息类型
    // void transfer_content(const std::string& user_ID, const CommandPtr& command);
};

using TS = TcpServer;

#endif