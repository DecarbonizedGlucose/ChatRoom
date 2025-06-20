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
private:
    json data;

public:
    Command() = delete;
    Command(Action action, const std::string& sender, const std::vector<std::string>& args);
    Command(Action action, const std::string& sender);
};

using ComPtr = std::shared_ptr<Command>;

#endif