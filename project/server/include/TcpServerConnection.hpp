#ifndef TCP_SERVER_CONNECTION_HPP
#define TCP_SERVER_CONNECTION_HPP

#include "../../io/include/Socket.hpp"
#include "../../io/include/reactor.hpp"
#include "dispatcher.hpp"
#include <memory>
#include <chrono>
#include <queue>
#include <vector>

class TcpServerConnection : public std::enable_shared_from_this<TcpServerConnection> {
private:
    event* read_event = nullptr; // 读事件
    event* write_event = nullptr; // 写事件
    std::shared_ptr<reactor> reactor_ptr = nullptr;
    ASocketPtr socket = nullptr; // 套接字
    std::queue<std::vector<char>> write_queue;

    std::chrono::steady_clock::time_point last_active;

    //std::string client_id;
    std::string user_ID;
    bool authed = false;

    Dispatcher* dispatcher = nullptr;

public:
    TcpServerConnection(std::shared_ptr<rea> reactor_ptr, Dispatcher* disp)
        : reactor_ptr(reactor_ptr), dispatcher(disp) {
        last_active = std::chrono::steady_clock::now();
    }

    void handle_read();
    void handle_write();
    void send(const std::string& data);
    void close();
    bool is_timed_out(int seconds) const;

    std::string get_user_id() const {
        return user_ID;
    }
};

using TcpServerConnectionPtr = std::shared_ptr<TcpServerConnection>;

#endif