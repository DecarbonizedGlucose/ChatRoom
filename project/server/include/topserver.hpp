#ifndef TOPSERVER_HPP
#define TOPSERVER_HPP

#include "TcpServer.hpp"
#include <memory>

class TopServer {
private:
    std::shared_ptr<TS> command_server = nullptr;
    std::shared_ptr<TS> data_server = nullptr;
};

#endif