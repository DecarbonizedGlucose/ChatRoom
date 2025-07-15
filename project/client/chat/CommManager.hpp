#ifndef COMM_MANAGER_HPP
#define COMM_MANAGER_HPP

#include "../../client/include/TcpClient.hpp"

class CommManager {
private:
    TcpClientPtr message_client;
    TcpClientPtr command_client;
    TcpClientPtr data_client;
public:
    CommManager() {
        // 这里初始化三个Client实例
        // 需要ip和port*3
        message_client = std::make_shared<TcpClient>(
            set_addr::client_addr[0].first, set_addr::client_addr[0].second);
        command_client = std::make_shared<TcpClient>(
            set_addr::client_addr[1].first, set_addr::client_addr[1].second);
        data_client = std::make_shared<TcpClient>(
            set_addr::client_addr[2].first, set_addr::client_addr[2].second);
    }
};


#endif // COMM_MANAGER_HPP