#ifndef CHATMESSAGE_HPP
#define CHATMESSAGE_HPP

#include <ctime>
#include <string>
#include "nlohmann/json.hpp"
#include "file.hpp"
#include <memory>
using json = nlohmann::json;

/*
一个Message:
{
    "sender": "",
    "receiver": "",
    "timestamp": 114514,
    "text": "",
    "pin": true,
    "payload": {一个文件}
}
*/
class ChatMessage {
public:
    json message_json;

    ChatMessage() = delete;
    ChatMessage(const std::string& sender, const std::string& receiver, const std::string& text = "", bool pin = false, const ChatFilePtr& cfile = nullptr);

    void settimestamp();
};

using ChatMessagePtr = std::shared_ptr<ChatMessage>;

ChatMessage::ChatMessage(const std::string& sender, const std::string& receiver, const std::string& text = "", bool pin = false, const ChatFilePtr& cfile = nullptr)
    : message_json({
        {"sender", sender},
        {"receiver", receiver},
        {"timestamp", std::time(nullptr)},
        {"text", text},
        {"pin", pin},
        {"payload", json::object()}} // ?
    ) {
    if (cfile) {
        message_json["payload"]["file_name"] = cfile->file->file_name;
        message_json["payload"]["file_size"] = cfile->file->file_size;
    }
}

void ChatMessage::settimestamp() {
    message_json["timestamp"] = std::time(nullptr);
}

#endif