#include "../include/message.hpp"
#include <stdexcept>

/* ----- base message ----- */

Base_Message::Base_Message() 
    : timestamp(std::time(nullptr)) {} // 初始化时间戳为当前时间

/* ----- text message ----- */

TMes::Text_Message(const std::string& content) 
    : Base_Message(), content(content) {}

TMes::Text_Message(const char* str) 
    : Base_Message(), content(std::string(str)) {}

/* ----- file message ----- */

FMes::File_Message(const std::string& file_name)
    : Base_Message(), file_name(file_name) {}

FMes::File_Message(const FilePtr& file) : Base_Message(), file(file) {
    if (file) {
        file_name = file->file_name;
        file_size = file->file_size;
    }
    else {
        throw std::runtime_error("File pointer is null");
    }
}
