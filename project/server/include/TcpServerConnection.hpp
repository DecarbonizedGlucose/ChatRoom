#pragma once

#include "../../global/abstract/datatypes.hpp"
#include <string>

class event;
class reactor;
class Dispatcher;
class AcceptedSocket;

class TcpServerConnection : public std::enable_shared_from_this<TcpServerConnection> {
public:
    event* read_event = nullptr; // 读事件
    event* write_event = nullptr; // 写事件
    reactor* reactor_ptr = nullptr;
    AcceptedSocket* socket = nullptr; // 套接字
    //safe_queue<std::string> write_queue;
    Dispatcher* dispatcher = nullptr;
    std::string user_ID;
    DataType to_send_type = DataType::None;

    TcpServerConnection(reactor* reactor_ptr, Dispatcher* disp);
    ~TcpServerConnection();

    void set_send_type(DataType type);
};
