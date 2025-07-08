#ifndef COMMAND_HPP
#define COMMAND_HPP

#include "action.hpp"
#include "nlohmann/json.hpp"
#include <memory>
#include <vector>
using json = nlohmann::json;

/*
    一个command对象代表一个命令实例化对象
    命令具体内容由data字段存储

data: {
    "action": (int), // 命令类型, 见action.hpp
    "sender": "user_ID", // 发送者ID
    "args": [
        "arg1", // 可选参数, string类型
        "arg2",
        ...
    ]
}
*/
class Command {
public:
    json data;
    Command() = delete;
    Command(Action action, const std::string& sender, const std::vector<std::string>& args);
    Command(Action action, const std::string& sender);
};

using ComPtr = std::shared_ptr<Command>;

Command::Command(Action action, const std::string& sender, const std::vector<std::string>& args)
    : data({
        {"action", static_cast<int>(action)},
        {"sender", sender},
        {"args", args} // 使用传入的参数列表
    }) {}

Command::Command(Action action, const std::string& sender)
    : data({
        {"action", static_cast<int>(action)},
        {"sender", sender},
        {"args", json::array()} // 空数组
    }) {}

#endif