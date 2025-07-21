#pragma once

class Dispatcher;
class RedisController;
class MySQLController;
class TcpServer;
class thread_pool;

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

    void launch();
    void stop();
};