#include "../include/time_utils.hpp"
#include "datatypes.hpp"
#include <iostream>
#include <ctime>

/* ---------- ChatMessage ---------- */

ChatMessage create_chat_message(
    const std::string& sender,
    const std::string& receiver,
    const bool is_group_msg,
    const std::int64_t timestamp,
    const std::string& text,
    const bool pin,
    const std::string& file_name,
    const std::size_t file_size,
    const std::string& file_hash
) {
    ChatMessage msg;
    msg.set_sender(sender);
    msg.set_receiver(receiver);
    msg.set_is_group(is_group_msg);
    msg.set_timestamp(timestamp);
    msg.set_text(text);
    msg.set_pin(pin);
    // 如果有文件附件, 设置 FilePayload
    if (pin && !file_name.empty()) {
        FilePayload* payload = msg.mutable_payload();
        payload->set_file_name(file_name);
        payload->set_file_size(file_size);
        payload->set_file_hash(file_hash);
    }
    return msg;
}

std::string get_message_string(const ChatMessage& msg) {
    google::protobuf::Any any;
    any.PackFrom(msg);
    Envelope env;
    env.mutable_payload()->CopyFrom(any);
    std::string env_out;
    env.SerializeToString(&env_out);
    return env_out;
}

std::string create_message_string(
    const std::string& sender,
    const std::string& receiver,
    const bool is_group_msg,
    const std::int64_t timestamp,
    const std::string& text,
    const bool pin,
    const std::string& file_name,
    const std::size_t file_size,
    const std::string& file_hash
) {
    auto msg = create_chat_message(sender, receiver, is_group_msg, timestamp, text,
                                   pin, file_name, file_size, file_hash);
    return get_message_string(msg);
}

ChatMessage get_chat_message(const std::string& proto_str) {
    Envelope env;
    if (proto_str.empty()) {
        return ChatMessage();
    }
    if (!env.ParseFromString(proto_str)) {
        throw std::runtime_error("Failed to parse Envelope from received data");
    }
    const google::protobuf::Any& any = env.payload();
    ChatMessage msg;
    if (!any.UnpackTo(&msg)) {
        throw std::runtime_error("Failed to unpack Any to ChatMessage");
    }
    return msg;
}

/* ---------- CommandRequest ---------- */

CommandRequest create_command(
    Action action,
    const std::string& sender,
    std::initializer_list<std::string> args
) {
    CommandRequest cmd;
    cmd.set_action(static_cast<int>(action));
    cmd.set_sender(sender);
    for (const auto& arg : args) {
        cmd.add_args(arg);
    }
    return cmd;
}

CommandRequest create_command(
    Action action,
    const std::string& sender,
    const std::unordered_map<std::string, bool>& args
) {
    CommandRequest cmd;
    cmd.set_action(static_cast<int>(action));
    cmd.set_sender(sender);
    for (const auto& arg : args) {
        cmd.add_args(arg.first);
    }
    return cmd;
}

std::string get_command_string(const CommandRequest& cmd) {
    google::protobuf::Any any;
    any.PackFrom(cmd);
    Envelope env;
    env.mutable_payload()->CopyFrom(any); // 只用 CopyFrom, 不要 PackFrom
    //std::cout << "打包测试类型[" << env.payload().type_url() << "]" << std::endl; // 调试输出
    std::string env_out;
    env.SerializeToString(&env_out);
    return env_out;
}

std::string create_command_string(
    Action action,
    const std::string& sender,
    std::initializer_list<std::string> args
) {
    auto cmd = create_command(action, sender, args);
    return get_command_string(cmd);
}

std::string create_command_string(
    Action action,
    const std::string& sender,
    const std::unordered_map<std::string, bool>& args
) {
    auto cmd = create_command(action, sender, args);
    return get_command_string(cmd);
}

CommandRequest get_command_request(const std::string& proto_str) {
    Envelope env;
    if (proto_str.empty()) {
        return CommandRequest();
    }
    if (!env.ParseFromString(proto_str)) {
        throw std::runtime_error("Failed to parse Envelope from received data");
    }
    const google::protobuf::Any& any = env.payload();
    CommandRequest cmd;
    if (!any.UnpackTo(&cmd)) {
        throw std::runtime_error("Failed to unpack Any to CommandRequest");
    }
    return cmd;
}

/* ---------- FileChunk ---------- */

FileChunk create_file_chunk(
    const std::string& file_id,
    const std::vector<char>& data,
    size_t chunk_index,
    size_t total_chunks,
    bool is_last_chunk
) {
    FileChunk chunk;
    chunk.set_file_id(file_id);
    chunk.set_data(data.data(), data.size());
    chunk.set_chunk_index(chunk_index);
    chunk.set_total_chunks(total_chunks);
    chunk.set_is_last_chunk(is_last_chunk);
    return chunk;
}

std::string get_file_chunk_string(const FileChunk& chunk) {
    google::protobuf::Any any;
    any.PackFrom(chunk);
    Envelope env;
    env.mutable_payload()->CopyFrom(any);
    std::string env_out;
    env.SerializeToString(&env_out);
    return env_out;
}

std::string create_file_chunk_string(
    const std::string& file_id,
    const std::vector<char>& data,
    size_t chunk_index,
    size_t total_chunks,
    bool is_last_chunk
) {
    auto chunk = create_file_chunk(file_id, data, chunk_index, total_chunks, is_last_chunk);
    return get_file_chunk_string(chunk);
}

FileChunk get_file_chunk(const std::string& proto_str) {
    Envelope env;
    if (!env.ParseFromString(proto_str)) {
        throw std::runtime_error("Failed to parse Envelope from received data");
    }
    const google::protobuf::Any& any = env.payload();
    FileChunk chunk;
    if (!any.UnpackTo(&chunk)) {
        throw std::runtime_error("Failed to unpack Any to FileChunk");
    }
    return chunk;
}

/* ---------- SyncItem ---------- */

SyncItem create_sync_item(
    SyncItem::SyncType type,
    const std::string& content,
    std::int64_t timestamp
) {
    SyncItem item;
    item.set_type(type);
    item.set_content(content);
    if (timestamp == 0) {
        timestamp = now_us();
    }
    item.set_timestamp(timestamp);
    return item;
}

std::string get_sync_string(const SyncItem& item) {
    google::protobuf::Any any;
    any.PackFrom(item);
    Envelope env;
    env.mutable_payload()->CopyFrom(any);
    std::string env_out;
    env.SerializeToString(&env_out);
    return env_out;
}

std::string create_sync_string(
    SyncItem::SyncType type,
    const std::string& content,
    std::int64_t timestamp
) {
    auto item = create_sync_item(type, content, timestamp);
    return get_sync_string(item);
}

SyncItem get_sync_item(const std::string& proto_str) {
    Envelope env;
    if (!env.ParseFromString(proto_str)) {
        throw std::runtime_error("Failed to parse Envelope from received data");
    }
    const google::protobuf::Any& any = env.payload();
    SyncItem item;
    if (!any.UnpackTo(&item)) {
        throw std::runtime_error("Failed to unpack Any to SyncItem");
    }
    return item;
}

/* ---------- OfflineMessage ---------- */

OfflineMessages create_offline_messages(const std::vector<ChatMessage>& messages) {
    OfflineMessages offline_msgs;
    for (const auto& msg : messages) {
        ChatMessage* new_msg = offline_msgs.add_messages();
        new_msg->CopyFrom(msg);
    }
    return offline_msgs;
}

std::string get_offline_messages_string(const OfflineMessages& offline_msgs) {
    google::protobuf::Any any;
    any.PackFrom(offline_msgs);
    Envelope env;
    env.mutable_payload()->CopyFrom(any);
    std::string env_out;
    env.SerializeToString(&env_out);
    return env_out;
}

std::string create_offline_messages_string(const std::vector<ChatMessage>& messages) {
    auto offline_msgs = create_offline_messages(messages);
    return get_offline_messages_string(offline_msgs);
}

OfflineMessages get_offline_messages(const std::string& proto_str) {
    Envelope env;
    if (!env.ParseFromString(proto_str)) {
        throw std::runtime_error("Failed to parse Envelope from received data");
    }
    const google::protobuf::Any& any = env.payload();
    OfflineMessages offline_msgs;
    if (!any.UnpackTo(&offline_msgs)) {
        throw std::runtime_error("Failed to unpack Any to OfflineMessages");
    }
    return offline_msgs;
}