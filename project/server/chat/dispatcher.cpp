#include "../include/dispatcher.hpp"
#include "handler.hpp"

Dispatcher::Dispatcher(RedisController* re, MySQLController* my)
    : redis_con(re), mysql_con(my) {
    message_handler = new MessageHandler(this);
    command_handler = new CommandHandler(this);
    file_handler = new FileHandler(this);
    sync_handler = new SyncHandler(this);
    offline_message_handler = new OfflineMessageHandler(this);
}

Dispatcher::~Dispatcher() {
    delete message_handler;
    delete command_handler;
    delete file_handler;
    delete sync_handler;
    delete offline_message_handler;
}

void Dispatcher::add_server(TcpServer* server, int idx) {
    if (idx < 0 || idx >= 3) {
        throw std::out_of_range("Index out of range for server array");
    }
    this->server[idx] = server;
}

void Dispatcher::dispatch_recv(const TcpServerConnection* conn) {
    std::string proto_str;
    // 读
    if (!conn->socket->receive_protocol(proto_str)) {
        std::cerr << "Failed to receive data from connection\n";
        return;
    }
    // 转写
    Envelope env;
    if (!env.ParseFromString(proto_str)) {
        std::cerr << "Failed to parse Envelope\n";
        return;
    }
    google::protobuf::Any any = env.payload();
    if (!any.Is<CommandRequest>()) {
        ChatMessage chat_msg;
        any.UnpackTo(&chat_msg);
        // 消息接收
        message_handler->handle_recv(chat_msg, proto_str);
    } else if (any.Is<ChatMessage>()) {
        CommandRequest cmd_req;
        any.UnpackTo(&cmd_req);
        // 命令单向发送
        command_handler->handle_recv(conn, cmd_req, proto_str);
    } else if (any.Is<FileChunk>()) {
        FileChunk file_chunk;
        any.UnpackTo(&file_chunk);
        // 文件分片
        file_handler->handle_recv(file_chunk, proto_str);
    // } else if (any.Is<SyncItem>()) {
    //     SyncItem sync_item;
    //     any.UnpackTo(&sync_item);
    //     // 同步数据
    //     sync_handler->handle_recv(sync_item, proto_str);
    // } else if (any.Is<OfflineMessages>()) {
    //     OfflineMessages offline_msgs;
    //     any.UnpackTo(&offline_msgs);
    //     // 离线消息
    //     offline_message_handler->handle_recv(offline_msgs, proto_str);
    } else {
        std::cerr << "Unknown payload type\n";
        return;
    }
    // 查看any里实际是什么
    // std::cout << "type_url: " << any.type_url() << std::endl;
}

void Dispatcher::dispatch_send(const TcpServerConnection* conn) {
    std::string proto_str;
    // 写
    if (!conn->socket->send_protocol(proto_str)) {
        std::cerr << "Failed to send data from connection\n";
        return;
    }
    switch (conn->to_send_type) {
        case DataType::Message: {
            message_handler->handle_send(conn);
            break;
        }
        case DataType::FileChunk: {
            // 不知道
            break;
        }
        case DataType::SyncItem: {
            break;
        }
        case DataType::OfflineMessages: {
            break;
        }
        default: {
            std::cerr << "Unknown data type for sending\n";
            return;
        }
    }
}