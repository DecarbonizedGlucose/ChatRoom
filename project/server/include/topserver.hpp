#ifndef TOPSERVER_HPP
#define TOPSERVER_HPP

#include "TcpServer.hpp"
#include <memory>

class TopServer {
private:
    std::shared_ptr<TS> command_server = nullptr;
    std::shared_ptr<TS> data_server = nullptr;
    std::shared_ptr<thread_pool> pool = nullptr; // 外援

public:
    TopServer(std::shared_ptr<thread_pool> pool);
    ~TopServer();

    void launch();
};

#endif