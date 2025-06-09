#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <ctime>
#include <string>

/*     基类消息     */

class Base_Message {
public:
    Base_Message();
    virtual ~Base_Message() = default;

    std::time_t timestamp; // 时间戳，记录消息发送的时间
};

Base_Message::Base_Message() 
    : timestamp(std::time(nullptr)) {} // 初始化时间戳为当前时间

/*     文本消息     */

class Text_Message : public Base_Message {
public:
    Text_Message() = delete;
    Text_Message(const std::string& content);
    Text_Message(const char* str);

    std::string content; // 文本内容
};

Text_Message::Text_Message(const std::string& content) 
    : Base_Message(), content(content) {}

Text_Message::Text_Message(const char* str) 
    : Base_Message(), content(std::string(str)) {}

/*     文件消息     */

class File_Message : public Base_Message {
public:

};

/*     图片消息     */

class Image_Message : public Base_Message {};

/*     卡片消息     */

class Card_Message : public Base_Message {};

#endif