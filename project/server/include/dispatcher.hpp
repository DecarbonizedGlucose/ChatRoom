#ifndef DISPATCHER_HPP
#define DISPATCHER_HPP

#include <memory>
#include <unordered_map>
#include <functional>
#include "TcpServer.hpp"
#include <google/protobuf/message.h>
#include "../../global/abstract/envelope.pb.h"
#include "../../global/abstract/command.pb.h"
#include "../../global/abstract/message.pb.h"
#include "../../global/abstract/data.pb.h"
#include "handler.hpp"

class Dispatcher {
public:
    //using Task = std::function<void()>;

    Dispatcher();

    void dispatch(const std::string& proto_str, const TcpServerConnectionPtr& conn);
    void process();

private:
    CommandHandler* cmd_handler = nullptr;
    MessageHandler* msg_handler = nullptr;
    FileHandler* file_handler = nullptr;
    SyncHandler* sync_handler = nullptr;
    OfflineMessageHandler* offline_msg_handler = nullptr;
};

#endif