#pragma once
#include <memory>

class Dispatcher;
class RedisController;
class TcpServer;
class thread_pool;

class TopServer {
public:
    std::unique_ptr<thread_pool> pool = nullptr;
    TcpServer* message_server = nullptr;
    TcpServer* command_server = nullptr;
    TcpServer* data_server = nullptr;
    Dispatcher* disp = nullptr;
    RedisController* redis = nullptr;

    TopServer();
    ~TopServer();

    void launch();
};