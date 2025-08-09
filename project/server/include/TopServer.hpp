#pragma once

#include <string>

class Dispatcher;
class RedisController;
class MySQLController;
class TcpServer;
class thread_pool;

namespace mysql_config {
    extern std::string host;
    extern std::string user;
    extern std::string password;
    extern std::string dbname;
    extern unsigned int port;
    void config(std::string file);
}

class TopServer {
public:
    thread_pool* pool = nullptr;
    TcpServer* message_server = nullptr;
    TcpServer* command_server = nullptr;
    TcpServer* data_server = nullptr;
    Dispatcher* disp = nullptr;
    RedisController* redis = nullptr;
    MySQLController* mysql = nullptr;

    TopServer();
    ~TopServer();

    bool launch();
    void stop();
};