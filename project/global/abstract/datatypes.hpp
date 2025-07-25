#pragma once
#include <initializer_list>
#include <string>
#include <google/protobuf/message.h>
#include <google/protobuf/any.pb.h>
#include "envelope.pb.h"
#include "message.pb.h"
#include "command.pb.h"
//#include "file.pb.h"
#include "data.pb.h"
#include "envelope.pb.h"
#include "../include/action.hpp"

enum class DataType {
    None,
    Message,
    Command,
    FileChunk,       // in any
    SyncItem,        // in any
    OfflineMessages  // in any
};

/* ---------- Message ---------- */

ChatMessage create_chat_message(
    const std::string& sender,
    const std::string& receiver,
    const bool is_group_msg,
    const std::string& text,
    const bool pin = false,
    const std::string& file_name = "",
    const std::size_t file_size = 0,
    const std::string& file_hash = ""
);

std::string get_message_string(const ChatMessage& msg);

std::string create_message_string(
    const std::string& sender,
    const std::string& receiver,
    const bool is_group_msg,
    const std::string& text,
    const bool pin = false,
    const std::string& file_name = "",
    const std::size_t file_size = 0,
    const std::string& file_hash = ""
);

ChatMessage get_chat_message(const std::string& proto_str);

/* ---------- Command ---------- */

// create CommandRequest with Action and args
CommandRequest create_command(
    Action action,
    const std::string& sender,
    std::initializer_list<std::string> args
);

// trans CommandRequest to string
std::string get_command_string(const CommandRequest& cmd);

// create CommandRequest string
std::string create_command_string(
    Action action,
    const std::string& sender,
    std::initializer_list<std::string> args
);

// trans string to CommandRequest
CommandRequest get_command_request(const std::string& proto_str);

/* ---------- FileChunk ---------- */

/* ---------- SyncItem ----------*/

SyncItem create_sync_item(
    SyncItem::SyncType type,
    const std::string& target_id,
    const std::string& content,
    std::time_t timestamp = 0
);

std::string get_sync_string(const SyncItem& item);

std::string create_sync_string(
    SyncItem::SyncType type,
    const std::string& target_id,
    const std::string& content,
    std::time_t timestamp = 0
);

SyncItem get_sync_item(const std::string& proto_str);

/* ---------- OfflineMessages ---------- */