#include "../include/message.hpp"

// Message::Message(const std::string& sender, const std::string& receiver, const std::string& text = "", bool pin = false, const ChatFilePtr& cfile = nullptr) {
// }

// void Message::settimestamp() {
//     if (message_ptr) {
//         message_ptr->set_timestamp(static_cast<int64_t>(std::time(nullptr)));
//     }
// }

ChatMessagePtr getChatMessagePtr(const std::string& sender,
    const std::string& receiver, const std::string& text,
    bool pin, const ChatFilePtr& cfileptr,
    size_t file_size) {
    auto message = std::make_shared<ChatMessage>();
    message->set_sender(sender);
    message->set_receiver(receiver);
    message->set_timestamp(static_cast<int64_t>(std::time(nullptr)));
    message->set_text(text);
    message->set_pin(pin);
    if (cfileptr) {
        auto file = message->mutable_payload();
        file->set_file_name(cfileptr->file_name());
        file->set_file_size(file_size);
        file->set_file_hash(cfileptr->file_hash());
    }
    return message;
}