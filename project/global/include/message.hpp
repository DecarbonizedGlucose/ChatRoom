#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <ctime>
#include <string>
#include "file.hpp"

/*     基类消息     */

class Base_Message {
public:
    Base_Message();
    virtual ~Base_Message() = default;

    std::time_t timestamp; // 时间戳，记录消息发送的时间
};

/*     文本消息     */

class Text_Message : public Base_Message {
public:
    std::string content; // 文本内容

    Text_Message() = delete;
    Text_Message(const std::string& content);
    Text_Message(const char* str);
};

/*     文件消息     */

class File_Message : public Base_Message {
public:
    FilePtr file = nullptr;
    std::string file_name;
    off_t file_size; 

    File_Message() = delete;
    File_Message(const std::string& file_name);                                      // 接受对方发送的文件, 未下载时使用
    // File_Message(const std::string& local_path);                                     // 发送文件时使用
    // File_Message(const std::string& file_name, const std::string& local_path);       // 发送文件时使用, 支持指定文件名
    File_Message(const FilePtr& file);                                               // 接受对方发送的文件, 已下载时使用
};

/*     图片消息     */

class Image_Message : public Base_Message {};

/*     卡片消息     */

class Card_Message : public Base_Message {};

using TMes = Text_Message;
using FMes = File_Message;
using IMes = Image_Message;
using CMes = Card_Message;

#endif