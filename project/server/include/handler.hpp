#ifndef HANDLER_HPP
#define HANDLER_HPP

#include <google/protobuf/message.h>
#include <memory>
#include "TcpServer.hpp"
#include <google/protobuf/message.h>
#include "../../global/abstract/envelope.pb.h"
#include "../../global/abstract/command.pb.h"
#include "../../global/abstract/message.pb.h"
#include "../../global/abstract/data.pb.h"
#include "TcpServerConnection.hpp"
#include "dispatcher.hpp"
#include "../../global/include/action.hpp"
#include <string>
#include <vector>
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

    void handle(const ChatMessage& message, const TcpServerConnectionPtr& conn) {
        // 处理聊天消息
        std::string sender = message.sender();
        std::string receiver = message.receiver();
        bool is_group = message.is_group();
        // 只需要知道这些
        // 从redis拿出地址发送即可
    }
};

/* -------------- Command -------------- */

class CommandHandler : public Handler {
public:
    CommandHandler(Dispatcher* dispatcher) : Handler(dispatcher) {}

    void handle(const CommandRequest& command, const TcpServerConnectionPtr& conn) {
        // 处理命令
        Action action = (Action)command.action();

    }
};

/* -------------- Data -------------- */

class FileHandler : public Handler {
public:
    FileHandler(Dispatcher* dispatcher) : Handler(dispatcher) {}

    void handle(const FileChunk& file_chunk, const TcpServerConnectionPtr& conn) {
        // 处理文件分片
    }
};

class SyncHandler : public Handler {
public:
    SyncHandler(Dispatcher* dispatcher) : Handler(dispatcher) {}

    void handle(const SyncItem& sync_item, const TcpServerConnectionPtr& conn) {
        // 处理同步数据
    }
};

class OfflineMessageHandler : public Handler {
public:
    OfflineMessageHandler(Dispatcher* dispatcher) : Handler(dispatcher) {}

    void handle(const OfflineMessages& offline_messages, const TcpServerConnectionPtr& conn) {
        // 处理离线消息
    }
};

#endif