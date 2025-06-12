#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <ctime>
#include <string>
#include <memory>
#include "file.hpp"

/*
 * 消息传输格式：
 * [Type][Sender][Receiver][Timestamp][Size][Content]
 *  1B     8B        8B        8B       8B     NB (size_t)
 * Total: (33+N) Bytes
 * 
 * Text:
 * [Size] = [Text Size (8B)]
 * [Content] = [Text Content]
 * File:
 * [Size] = [File Name Size + File Data Size (8B)]
 * [Content] = [File Name Size (8B)] + [File Name] + [File Data]
*/

/*     基类消息     */

class Base_Message {
public:
    Base_Message();
    virtual ~Base_Message() = default;

    static const char type;
    std::time_t timestamp; // 时间戳，记录消息发送的时间
    u_int8_t sender;
    u_int8_t receiver;
    size_t size; // 消息内容的大小
};

const char Base_Message::type = 'M';

/*     文本消息     */

class Text_Message : public Base_Message {
public:
    static const char type;
    std::string content; // 文本内容

    Text_Message() = delete;
    Text_Message(const std::string& content);
    Text_Message(const char* str);
};

const char Text_Message::type = 'T';

/*     文件消息     */

class File_Message : public Base_Message {
public:
    static const char type;
    FilePtr file = nullptr;
    std::string file_name;
    off_t file_size; 

    File_Message() = delete;
    File_Message(const std::string& file_name);                                      // 接受对方发送的文件, 未下载时使用
    // File_Message(const std::string& local_path);                                     // 发送文件时使用
    // File_Message(const std::string& file_name, const std::string& local_path);       // 发送文件时使用, 支持指定文件名
    File_Message(const FilePtr& file);                                               // 接受对方发送的文件, 已下载时使用
};

const char File_Message::type = 'F';

/*     图片消息     */

class Image_Message : public Base_Message {
public:
    static const char type;
};

const char Image_Message::type = 'I';

/*     卡片消息     */

class Card_Message : public Base_Message {
public:
    static const char type;
};

const char Card_Message::type = 'C';

/*     别名     */

using Mes = Base_Message;
using TMes = Text_Message;
using FMes = File_Message;
using IMes = Image_Message;
using CMes = Card_Message;

using MesPtr = std::shared_ptr<Base_Message>;
using TMesPtr = std::shared_ptr<Text_Message>;
using FMesPtr = std::shared_ptr<File_Message>;
using IMesPtr = std::shared_ptr<Image_Message>;
using CMesPtr = std::shared_ptr<Card_Message>;

#endif