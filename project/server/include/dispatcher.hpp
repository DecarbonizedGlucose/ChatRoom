#pragma once

#include <memory>
#include <unordered_map>
#include <functional>
#include "TcpServer.hpp"
#include <google/protobuf/message.h>
#include "../../global/abstract/envelope.pb.h"
#include "../../global/abstract/command.pb.h"
#include "../../global/abstract/message.pb.h"
#include "../../global/abstract/data.pb.h"
#include "../include/redis.hpp"

class TcpServer;
class TcpServerConnection;
class CommandHandler;
class MessageHandler;
class FileHandler;
class SyncHandler;
class OfflineMessageHandler;

class Dispatcher {
public:
    //using Task = std::function<void()>;
    TcpServer* server[3];
    RedisController* redis_con = nullptr;

    Dispatcher(RedisController* re);
    ~Dispatcher();

    void add_server(TcpServer* server, int idx);
    void dispatch_recv(const TcpServerConnection* conn);
    void dispatch_send(const TcpServerConnection* conn);

private:
    CommandHandler* command_handler = nullptr;
    MessageHandler* message_handler = nullptr;
    FileHandler* file_handler = nullptr;
    SyncHandler* sync_handler = nullptr;
    OfflineMessageHandler* offline_message_handler = nullptr;
};
