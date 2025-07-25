#include "../include//CommManager.hpp"
#include "../include/TopClient.hpp"
#include "../include/TcpClient.hpp"
#include "../../global/include/threadpool.hpp"
#include "../../global/abstract/datatypes.hpp"
#include "../../global/include/logging.hpp"
#include "../include/sqlite.hpp"

CommManager::CommManager(TopClient* client)
    : top_client(client) {
    clients[0] = top_client->message_client;
    clients[1] = top_client->command_client;
    clients[2] = top_client->data_client;
    sqlite_con = new SQLiteController("chat_database.db");
}

/* ---------- Pure Input & Output ---------- */

std::string CommManager::read(int idx) {
    std::string proto;
    clients[idx]->socket->receive_protocol(proto);
    return proto;
}

auto CommManager::read_async(int idx) {
    return top_client->pool->submit([this, idx](){
        std::string proto;
        clients[idx]->socket->receive_protocol(proto);
        return proto;
    });
}

void CommManager::send(int idx, const std::string& proto) {
    clients[idx]->socket->send_protocol(proto);
}

auto CommManager::send_async(int idx, const std::string& proto) {
    return top_client->pool->submit([this, idx, proto](){
        return clients[idx]->socket->send_protocol(proto);
    });
}

std::string CommManager::read_nb(int idx) {
    std::string proto;
    DataSocket::RecvState state;
    while (*cont) {
        state = clients[idx]->socket->receive_protocol_with_state(proto);
        switch (state) {
            case DataSocket::RecvState::Success: return proto;
            case DataSocket::RecvState::NoMoreData: continue;
            case DataSocket::RecvState::Disconnected: {
                log_info("Connection {} disconnected", idx);
                return "";
            }
            default: {
                log_error("Error receiving data from connection {}: {}", idx, proto);
                return "";
            }
        }
    }
    return "";
}

void CommManager::send_nb(int idx, const std::string& proto) {
    while (*cont) {
        if (clients[idx]->socket->send_protocol(proto)) {
            return;
        }
    }
    return;
}

/* ---------- Handlers ---------- */

// message

ChatMessage CommManager::handle_receive_message(bool nb) {
    std::string proto;
    if (!nb) proto = this->read(0);
    else proto = this->read_nb(0);
    return get_chat_message(proto);
}

void CommManager::handle_send_message(const std::string& sender, const std::string& receiver,
                                      bool is_group_msg, const std::string& text,
                                      bool pin, const std::string& file_name,
                                      std::size_t file_size, const std::string& file_hash,
                                      bool nb) {
    std::string env_out = create_message_string(sender, receiver, is_group_msg, text,
                                             pin, file_name, file_size, file_hash);
    if (!nb) this->send(0, env_out);
    else this->send_nb(0, env_out);
}

void CommManager::handle_manage_message(const ChatMessage& msg) {
    // 存到本地 (SQLite)
    // 根据打开的聊天窗口，决定是否需要显示
}

// command

CommandRequest CommManager::handle_receive_command(bool nb) {
    std::string proto;
    if (!nb) proto = this->read(1);
    else proto = this->read_nb(1);
    return get_command_request(proto);
}

void CommManager::handle_send_command(Action action, const std::string& sender,
                                      std::initializer_list<std::string> args,
                                      bool nb) {
    std::string env_out = create_command_string(action, sender, args);
    if (!nb) this->send(1, env_out);
    else this->send_nb(1, env_out);
}

void CommManager::handle_manage_command(const CommandRequest& cmd) {
    // 这里可以添加处理命令的逻辑
}

// file chunk

// sync item

// offline messages

// others

void CommManager::handle_send_id() {
    for (int i=0; i<3; ++i) {
        auto env_out = create_command_string(
            Action::Remember_Connection, user_ID, {std::to_string(i)});
        this->send(i, env_out);
    }
    log_info("Sent user ID to all servers");
}

/* ---------- Print ---------- */