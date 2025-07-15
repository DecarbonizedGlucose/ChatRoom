#ifndef TCP_SERVER_CONNECTION_HPP
#define TCP_SERVER_CONNECTION_HPP

#include "../../io/include/Socket.hpp"
#include "../../io/include/reactor.hpp"
#include "../../global/abstract/datatypes.hpp"
#include "../../global/include/safe_queue.hpp"
#include "dispatcher.hpp"
#include <memory>
#include <chrono>
#include <string>

class TcpServerConnection : public std::enable_shared_from_this<TcpServerConnection> {
public:
    event* read_event = nullptr; // 读事件
    event* write_event = nullptr; // 写事件
    std::shared_ptr<reactor> reactor_ptr = nullptr;
    ASocketPtr socket = nullptr; // 套接字
    safe_queue<std::string> write_queue;
    Dispatcher* dispatcher = nullptr;
    std::chrono::steady_clock::time_point last_active;
    //std::string client_id;
    std::string user_ID;
    //bool authed = false;
    DataType to_send_type = DataType::None;

    TcpServerConnection(std::shared_ptr<rea> reactor_ptr, Dispatcher* disp)
        : reactor_ptr(reactor_ptr), dispatcher(disp) {
        last_active = std::chrono::steady_clock::now();
    }

    bool is_timed_out(int seconds) const; // ???

    std::string get_user_id() const {
        return user_ID;
    }
};

using TcpServerConnectionPtr = std::shared_ptr<TcpServerConnection>;

#endif