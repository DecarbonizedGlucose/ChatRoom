#ifndef HANDLER_HPP
#define HANDLER_HPP

#include <google/protobuf/message.h>
#include "../../global/abstract/envelope.pb.h"
#include "../../global/abstract/command.pb.h"
#include "../../global/abstract/message.pb.h"
#include "../../global/abstract/data.pb.h"
#include "../../global/include/action.hpp"
#include "TcpServerConnection.hpp"
#include "TcpServer.hpp"
#include "dispatcher.hpp"
#include <string>
#include <vector>
#include <memory>
#include <queue> // 扩展用，比如消息队列/任务队列

class Handler {
public:
    Dispatcher* disp = nullptr;
    Handler(Dispatcher* dispatcher) : disp(dispatcher) {}
};

/* -------------- Message -------------- */

class MessageHandler : public Handler {
public:
    MessageHandler(Dispatcher* dispatcher) : Handler(dispatcher) {}

    void handle_recv(const ChatMessage& message, const std::string& ostr) {
        // 处理聊天消息
        std::string sender = message.sender();
        std::string receiver = message.receiver();
        bool is_group = message.is_group();
        // 只需要知道这些
        // 从redis拿出地址发送即可
        auto recv_status = disp->redis_con->get_user_status(receiver);
        if (!recv_status.first) {
            // 用户不在线,需要上线后拉取
            // 不会喵
            // 可能要搞个消息队列
        } else {
            // 用户在线,直接发送
            auto it = disp->server[0]->user_connections.find(receiver);
            if (it != disp->server[0]->user_connections.end()) {
                // 找到了连接
                auto send_conn = it->second;
                // 发送消息
                // 先把proto注入buf
                send_conn->socket->set_write_buf(ostr);
                // 操作事件
                send_conn->write_event->add_to_reactor();
                // 更新最后活跃时间
                // send_conn->last_active = std::chrono::steady_clock::now();
                // 剩下的交给reactor和handler_send
            } else {
                // 没找到连接,可能是用户离线了,或者是bug
                // 需要处理离线消息
                // 可以考虑放入离线消息队列
            }
        }
    }

    void handle_send() {

    }
};

/* -------------- Command -------------- */

class CommandHandler : public Handler {
public:
    CommandHandler(Dispatcher* dispatcher) : Handler(dispatcher) {}

    void handle_recv(const CommandRequest& command, const std::string& ostr) {
        // 处理命令
        Action action = (Action)command.action();

    }
};

/* -------------- Data -------------- */

class FileHandler : public Handler {
public:
    FileHandler(Dispatcher* dispatcher) : Handler(dispatcher) {}

    void handle_recv(const FileChunk& file_chunk, const std::string& ostr) {
        // 处理文件分片
    }
    
    void handle_send(const FileChunk& file_chunk, const std::string& ostr) {
        // 处理文件分片发送
    }
};

class SyncHandler : public Handler {
public:
    SyncHandler(Dispatcher* dispatcher) : Handler(dispatcher) {}

    void handle_send(const SyncItem& sync_item, const std::string& ostr) {
        // 处理同步数据
    }
};

class OfflineMessageHandler : public Handler {
public:
    OfflineMessageHandler(Dispatcher* dispatcher) : Handler(dispatcher) {}

    void handle_recv(const OfflineMessages& offline_messages, const std::string& ostr) {
        // 处理离线消息
    }
};

#endif