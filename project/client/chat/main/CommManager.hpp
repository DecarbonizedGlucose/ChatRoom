#ifndef COMM_MANAGER_HPP
#define COMM_MANAGER_HPP

// CommManager.hpp
// 向用户界面提供API

#include "../../include/TcpClient.hpp"
#include "../../include/TopClient.hpp"
#include <google/protobuf/message.h>
#include "../../../global/abstract/envelope.pb.h"
#include "../../../global/abstract/message.pb.h"
#include "../../../global/abstract/command.pb.h"
#include "../../../global/abstract/data.pb.h"
#include "../../../global/include/threadpool.hpp"

class CommManager {
private:
    TopClient* top_client;
    TcpClient* message_client;
    TcpClient* command_client;
    TcpClient* data_client;

public:
    CommManager(TopClient* client) : top_client(client) {
        message_client = top_client->message_client.get();
        command_client = top_client->command_client.get();
        data_client = top_client->data_client.get();
    }

/* ---------- Message ---------- */

    std::string read_message() {
        std::string message_proto;
        message_client->socket->receive_protocol(message_proto);
        return message_proto;
    }

    auto read_message_async() {
        return top_client->pool->submit([this](){
            std::string message_proto;
            message_client->socket->receive_protocol(message_proto);
            return message_proto;
        });
    }

    void send_message(const std::string& message_proto) {
        message_client->socket->send_protocol(message_proto);
    }

    auto send_message_async(const std::string& message_proto) {
        return top_client->pool->submit([this, message_proto](){
            return message_client->socket->send_protocol(message_proto);
        });
    }

/* ---------- Command ---------- */

    std::string read_command() {
        std::string command_proto;
        command_client->socket->receive_protocol(command_proto);
        return command_proto;
    }

    auto read_command_async() {
        return top_client->pool->submit([this](){
            std::string command_proto;
            command_client->socket->receive_protocol(command_proto);
            return command_proto;
        });
    }

    void send_command(const std::string& command_proto) {
        command_client->socket->send_protocol(command_proto);
    }

    auto send_command_async(const std::string& command_proto) {
        return top_client->pool->submit([this, command_proto](){
            return command_client->socket->send_protocol(command_proto);
        });
    }

/* ---------- Data --------- */

    void send_data(const std::string& data_proto) {
        data_client->socket->send_protocol(data_proto);
    }

    std::string read_data() {
        std::string data_proto;
        data_client->socket->receive_protocol(data_proto);
        return data_proto;
    }

    auto read_data_async() {
        return top_client->pool->submit([this](){
            std::string data_proto;
            data_client->socket->receive_protocol(data_proto);
            return data_proto;
        });
    }

    auto send_data_async(const std::string& data_proto) {
        return top_client->pool->submit([this, data_proto](){
            return data_client->socket->send_protocol(data_proto);
        });
    }
};

namespace handler {
    
}

#endif // COMM_MANAGER_HPP