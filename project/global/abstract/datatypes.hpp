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

/* ---------- Command ---------- */

// create CommandRequest with Action and args
CommandRequest create_command(
    Action action,
    const std::string& sender,
    std::initializer_list<std::string> args);

// trans CommandRequest to string
std::string get_command_string(const CommandRequest& cmd);

// create CommandRequest string
std::string create_command_string(
    Action action,
    const std::string& sender,
    std::initializer_list<std::string> args);

// trans string to CommandRequest
CommandRequest get_command_request(const std::string& proto_str);

/* ---------- FileChunk ---------- */

/* ---------- SyncItem ----------*/

/* ---------- OfflineMessages ---------- */