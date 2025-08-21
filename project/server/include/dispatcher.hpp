#pragma once

#include <memory>
#include <unordered_map>
#include <functional>
#include "../../global/abstract/datatypes.hpp"
#include "../include/redis.hpp"
#include "../include/mysql.hpp"
#include <chrono>

class TcpServer;
class TcpServerConnection;
class CommandHandler;
class MessageHandler;
class FileHandler;
class SyncHandler;
class OfflineMessageHandler;
class ConnectionManager;
class SFileManager;

class Dispatcher {
public:
    TcpServer* server[3];
    RedisController* redis_con = nullptr;
    MySQLController* mysql_con = nullptr;
    ConnectionManager* conn_manager = nullptr;
    SFileManager* file_manager = nullptr;

    Dispatcher(RedisController* re, MySQLController* my);
    ~Dispatcher();

    void flush_cached_messages();
    std::thread flush_message_thread;

    void add_server(TcpServer* server, int idx);
    void dispatch_recv(TcpServerConnection* conn);
    void dispatch_send(TcpServerConnection* conn);

private:
    bool running = false;

    CommandHandler* command_handler = nullptr;
    MessageHandler* message_handler = nullptr;
    FileHandler* file_handler = nullptr;
    SyncHandler* sync_handler = nullptr;
    OfflineMessageHandler* offline_message_handler = nullptr;
};
