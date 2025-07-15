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
#include "../database/redis.hpp"

class Dispatcher {
public:
    //using Task = std::function<void()>;
    TcpServer* server[3];
    RedisController* redis_con = nullptr;

    Dispatcher(RedisController* re);
    ~Dispatcher();

    void add_server(TcpServer* server, int idx);
    void dispatch_recv(const TcpServerConnectionPtr& conn);
    void dispatch_send(const TcpServerConnectionPtr& conn);

private:
    CommandHandler* command_handler = nullptr;
    MessageHandler* message_handler = nullptr;
    FileHandler* file_handler = nullptr;
    SyncHandler* sync_handler = nullptr;
    OfflineMessageHandler* offline_message_handler = nullptr;
};

#endif