#pragma once

#include "../../global/abstract/datatypes.hpp"
#include "../../global/include/safe_queue.hpp"
#include <string>
#include <atomic>

class event;
class reactor;
class Dispatcher;
class AcceptedSocket;

enum class SendStatus {
    Sending,
    Free,
    Waiting,
    Error
};

enum class ReceiveStatus {
    Receiving,
    Free,
    Waiting,
    Error
};

class TcpServerConnection : public std::enable_shared_from_this<TcpServerConnection> {
public:
    event* read_event = nullptr; // 读事件
    event* write_event = nullptr; // 写事件
    reactor* reactor_ptr = nullptr;
    AcceptedSocket* socket = nullptr; // 套接字
    safe_queue<std::pair<std::string, DataType>> write_queue;
    Dispatcher* dispatcher = nullptr;
    std::string user_ID;
    std::string temp_user_ID;
    DataType to_send_type = DataType::None;
    std::atomic<ReceiveStatus> receive_status = ReceiveStatus::Free; // 接收状态
    std::atomic<SendStatus> send_status = SendStatus::Free; // 发送状态

    TcpServerConnection(reactor* reactor_ptr, Dispatcher* disp);
    ~TcpServerConnection();

    void set_send_type(DataType type);
};
