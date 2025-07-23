
#include "datatypes.hpp"
#include <iostream>


CommandRequest create_command(
    Action action,
    const std::string& sender,
    std::initializer_list<std::string> args) {
    CommandRequest cmd;
    cmd.set_action(static_cast<int>(action));
    cmd.set_sender(sender);
    for (const auto& arg : args) {
        cmd.add_args(arg);
    }
    return cmd;
}

std::string get_command_string(const CommandRequest& cmd) {
    google::protobuf::Any any;
    any.PackFrom(cmd);
    Envelope env;
    env.mutable_payload()->CopyFrom(any); // 只用 CopyFrom，不要 PackFrom
    std::cout << "打包测试类型[" << env.payload().type_url() << "]" << std::endl; // 调试输出
    std::string env_out;
    env.SerializeToString(&env_out);
    return env_out;
}

std::string create_command_string(
    Action action,
    const std::string& sender,
    std::initializer_list<std::string> args) {
    auto cmd = create_command(action, sender, args);
    return get_command_string(cmd);
}

CommandRequest get_command_request(std::string& proto_str) {
    Envelope env;
    if (!env.ParseFromString(proto_str)) {
        throw std::runtime_error("Failed to parse Envelope from received data");
    }
    const google::protobuf::Any& any = env.payload();
    CommandRequest cmd;
    if (!any.UnpackTo(&cmd)) {
        throw std::runtime_error("Failed to unpack Any to CommandRequest");
    }
    std::cout << "解包测试类型[" << any.type_url() << "]" << std::endl; // 调试输出
    return cmd;
}
