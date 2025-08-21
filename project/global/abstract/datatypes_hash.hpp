#pragma once
#include "datatypes.hpp"
#include <functional>
#include <string>

bool is_same(const ChatMessage& lhs, const ChatMessage& rhs) {
    return lhs.sender() == rhs.sender() &&
           lhs.receiver() == rhs.receiver() &&
           lhs.is_group() == rhs.is_group() &&
           lhs.timestamp() == rhs.timestamp() &&
           lhs.text() == rhs.text() &&
           lhs.pin() == rhs.pin() &&
           lhs.payload().file_name() == rhs.payload().file_name() &&
           lhs.payload().file_size() == rhs.payload().file_size() &&
           lhs.payload().file_hash() == rhs.payload().file_hash();
}

namespace std {
    template <>
    struct hash<ChatMessage> {
        std::size_t operator()(const ChatMessage& msg) const {
            std::size_t h1 = std::hash<std::string>{}(msg.sender());
            std::size_t h2 = std::hash<std::string>{}(msg.receiver());
            std::size_t h3 = std::hash<int64_t>{}(msg.timestamp());
            std::size_t h4 = std::hash<std::string>{}(msg.text());
            std::size_t h5 = std::hash<bool>{}(msg.pin());
            std::size_t h6 = std::hash<std::string>{}(msg.payload().file_name());
            std::size_t h7 = std::hash<std::size_t>{}(msg.payload().file_size());
            std::size_t h8 = std::hash<std::string>{}(msg.payload().file_hash());
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4) ^ (h6 << 5) ^ (h7 << 6) ^ (h8 << 7);
        }
    };
}