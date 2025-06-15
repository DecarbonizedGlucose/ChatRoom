#include "../include/command.hpp"

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

