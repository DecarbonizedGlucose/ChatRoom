#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <ctime>
#include <string>
#include <google/protobuf/message.h>
#include "../abstract/message.pb.h"
#include "file.hpp"
#include <memory>

using ChatMessage = ChatMessage;
using ChatMessagePtr = std::shared_ptr<ChatMessage>;

/*
一个Message:
{
    "sender": "",
    "receiver": "",
    "timestamp": 114514,
    "text": "",
    "pin": true,
    "payload": {
        "file_name": "file.txt",
        "file_size": 123456,
        "file_hash": "文件哈希值"
    }
}
*/
// class Message {
// public:
//     FilePtr file = nullptr; // 携带的文件
//     ChatMessagePtr message_ptr = nullptr;

//     Message() = delete;
//     Message(const std::string& sender, const std::string& receiver, const std::string& text = "", bool pin = false, const ChatFilePtr& cfileptr = nullptr);

//     void settimestamp();
// };

// using MessagePtr = std::shared_ptr<Message>;

// 那还要Message什么事？
//ChatMessagePtr getChatMessagePtr(const MessagePtr& message);
ChatMessagePtr getChatMessagePtr(
    const std::string& sender, const std::string& receiver,
    const std::string& text = "", bool pin = false,
    const ChatFilePtr& cfileptr = nullptr, size_t file_size);

#endif