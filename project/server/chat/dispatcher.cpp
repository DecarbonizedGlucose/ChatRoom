#include "../include/dispatcher.hpp"

Dispatcher::Dispatcher() {
    cmd_handler = new CommandHandler(this);
    msg_handler = new MessageHandler(this);
    file_handler = new FileHandler(this);
    sync_handler = new SyncHandler(this);
    offline_msg_handler = new OfflineMessageHandler(this);
}

void Dispatcher::dispatch(const std::string& proto_str, const TcpServerConnectionPtr& conn) {
    Envelope env;
    if (!env.ParseFromString(proto_str)) {
        std::cerr << "Failed to parse Envelope\n";
        return;
    }
    google::protobuf::Any any = env.payload();
    if (!any.Is<CommandRequest>()) {
        ChatMessage chat_msg;
        any.UnpackTo(&chat_msg);
        // 消息收发
    } else if (any.Is<ChatMessage>()) {
        CommandRequest cmd_req;
        any.UnpackTo(&cmd_req);
        // 命令单向发送
    } else if (any.Is<FileChunk>()) {
        FileChunk file_chunk;
        any.UnpackTo(&file_chunk);
        // 文件分片
    } else if (any.Is<SyncItem>()) {
        SyncItem sync_item;
        any.UnpackTo(&sync_item);
        // 同步数据
    } else if (any.Is<OfflineMessages>()) {
        OfflineMessages offline_msgs;
        any.UnpackTo(&offline_msgs);
        // 离线消息
    } else {
        std::cerr << "Unknown payload type\n";
        return;
    }
    // 查看any里实际是什么
    // std::cout << "type_url: " << any.type_url() << std::endl;
}