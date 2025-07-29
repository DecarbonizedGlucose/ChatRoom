#include "../include/winloop.hpp"
#include "../include/output.hpp"
#include "../../global/include/logging.hpp"
#include "../include/CommManager.hpp"
#include "../../global/abstract/datatypes.hpp"
#include "../../global/include/threadpool.hpp"
#include "../include/TcpClient.hpp"
#include "../include/sqlite.hpp"
#include <iostream>
#include <regex>
#include <chrono>
#include "../../global/include/time_utils.hpp"

namespace {
    // 密码合规性
    bool is_password_valid(const std::string& password) {
        // 8-16位，至少包含一个大写字母、小写字母、数字和特殊字符，且不能包含空格，在ascii 127范围内
        if (password.length() < 8 || password.length() > 16) {
            return false;
        }
        bool has_upper = false;
        bool has_lower = false;
        bool has_digit = false;
        bool has_special = false;
        for (char c : password) {
            if (c < 0 || c > 127) {
                return false;
            }
            if (std::isupper(c)) has_upper = true;
            if (std::islower(c)) has_lower = true;
            if (std::isdigit(c)) has_digit = true;
            if (std::ispunct(c)) has_special = true;
            if (std::isspace(c)) return false;
        }
        return has_upper && has_lower && has_digit && has_special;
    }

    // 用户名合规性
    bool is_username_valid(const std::string& user_ID) {
        // 只能包含字母、数字和下划线，长度在5-20位之间
        if (user_ID.length() < 5 || user_ID.length() > 20) {
            return false;
        }
        for (char c : user_ID) {
            if (!std::isalnum(c) && c != '_') {
                return false;
            }
        }
        return true;
    }

    // 密码求哈希
    std::string hash_password(const std::string& password) {
        // 这里可以使用更安全的哈希函数
        return std::to_string(std::hash<std::string>{}(password));
    }

    // 邮箱合规性
    bool is_email_valid(const std::string& email) {
        const std::regex pattern(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
        return std::regex_match(email, pattern);
    }

}

WinLoop::WinLoop(CommManager* comm, thread_pool* pool)
    : current_page(UIPage::Start), comm(comm), pool(pool) {}

WinLoop::~WinLoop() {
}

void WinLoop::dispatch_cmd(const CommandRequest& cmd) {
    Action action = static_cast<Action>(cmd.action());
    std::string sender = cmd.sender();
    auto args = cmd.args();
    switch (action) {
        case Action::Notify: { // 包罗各种通知
            comm->cache.notices.push(cmd);
            break;
        }
        case Action::Notify_Exist: {
            // 通知某个东西存在
            comm->cache.real_time_notices.push(cmd);
            break;
        }
        case Action::Notify_Not_Exist: {
            // 通知某个东西不存在
            comm->cache.real_time_notices.push(cmd);
            break;
        }
        case Action::Friend_Online: {
            // 好友上线
            comm->cache.friend_list[args[0]].online = true;
            break;
        }
        case Action::Friend_Offline: {
            // 好友下线
            comm->cache.friend_list[args[0]].online = false;
            break;
        }
        case Action::Accept_FReq: {
            auto friend_ID = cmd.sender();
            comm->cache.notices.push(cmd);
            if (comm->cache.friend_list.find(friend_ID) != comm->cache.friend_list.end()) {
                // 说明是上线前同意的，已经拉取过来
                break;
            }
            comm->handle_add_friend(friend_ID);
        }
        case Action::Refuse_FReq: {
            comm->cache.notices.push(cmd);
        }
        case Action::Add_Friend_Req: {
            comm->cache.requests.push(cmd);
            break;
        }
        case Action::Remove_Friend: {
            comm->cache.notices.push(cmd);
            comm->handle_remove_friend(sender);
            break;
        }
        case Action::Join_Group_Req: {
            // 如果用户是管理员，有人申请加群的申请
            comm->cache.requests.push(cmd);
            break;
        }
        case Action::Accept_GReq: {
            // 加入群组的请求
            comm->cache.notices.push(cmd);
            // 这里面要拉取群成员名单
            comm->handle_join_group(comm->cache.user_ID, cmd.args(1));
            break;
        }
        case Action::Refuse_GReq: {
            // 拒绝加入群组的请求
            comm->cache.notices.push(cmd);
            break;
        }
        case Action::Leave_Group: { // 自己跑了
            break;
        }
        case Action::Disband_Group: {
            comm->cache.notices.push(cmd);
            // 还要有磁盘IO
            break;
        }
        case Action::Invite_To_Group_Req: {
            // 邀请加入群组的请求
            // 同意后自动发送入群申请
            comm->cache.requests.push(cmd);
            break;
        }
        case Action::Remove_From_Group: { // 被踢
            // 管理员会受到通知
            comm->cache.notices.push(cmd);
            // 还要有磁盘IO
            break;
        }
        case Action::Add_Admin: {
            comm->cache.notices.push(cmd);
            // 还有磁盘IO
            break;
        }
        case Action::Remove_Admin: {
            // 管理员会收到通知，被移除的管理员也会
            comm->cache.notices.push(cmd);
            // 还要有磁盘IO
            break;
        }
        default: {
            log_error("Unknown action in command: {}", static_cast<int>(action));
            return;
        }
    }
}

void WinLoop::run() {
    running = true;
    while (running) { // 状态机比那循环嵌套好看多了
        switch (current_page) {
            case UIPage::Start:
                start_loop();
                break;
            case UIPage::Login:
                login_loop();
                break;
            case UIPage::Register:
                register_loop();
                break;
            case UIPage::Main:
                main_loop();
                break;
            case UIPage::Message:
                message_loop();
                break;
            case UIPage::Contacts:
                contacts_loop();
                break;
            case UIPage::Show_Notices:
                notice_loop();
                break;
            case UIPage::Manage_Requests:
                request_loop();
                break;
            case UIPage::Add_Person:
                add_person_loop();
                break;
            case UIPage::Join_Group:
                join_group_loop();
                break;
            case UIPage::My_Lists:
                my_lists_loop();
                break;
            case UIPage::My:
                my_loop();
                break;
            case UIPage::Exit:
                comm->handle_send_command(Action::Sign_Out, comm->cache.user_ID, {}, false);
                running = false;
                break;
            default:
                log_error("Unknown page state: {}", static_cast<int>(current_page));
        }
    }
}

void WinLoop::stop() {
    running = false;
}

/* ---------- 页面处理 ---------- */

void WinLoop::start_loop() {
    sclear();
    draw_start(output_mutex);
    handle_start_input();
}

void WinLoop::login_loop() { // 改进用email/ID均可登录，并返回另一个
    while (1) {
        sclear();
        draw_login(output_mutex, 1);
        std::string user, password;
        std::getline(std::cin, user);
        if (user.empty()) { // 返回
            switch_to(UIPage::Start);
            return;
        }
        draw_login(output_mutex, 2);
        while (1) {
            std::getline(std::cin, password);
            if (user.empty() || password.empty()) {
                std::cout << "不能输入空字符串" << std::endl;
                print_input_sign();
                continue;
            }
            break;
        }
        auto password_hash = hash_password(password);
        // 传给服务器
        comm->handle_send_command(Action::Sign_In, user, {password_hash}, false);
        // 读取服务器响应
        CommandRequest resp = comm->handle_receive_command(false);
        if (resp.action() == static_cast<int>(Action::Accept_Login)) {
            std::cout << "登录成功！" << std::endl;
            if (is_email_valid(user)) {
                comm->cache.user_email = user;
                comm->cache.user_ID = resp.args(0);
            } else {
                comm->cache.user_ID = user;
                comm->cache.user_email = resp.args(0);
            }
            comm->cache.user_password_hash = password_hash;
            main_init();
            switch_to(UIPage::Main);
            return;
        } else { // Refused
            std::cout << "登录失败" << resp.args(0) << std::endl;
            pause();
            continue;
        }
    }
}

void WinLoop::register_loop() {
    while (1) {
        sclear();
        std::string email, veri_code, username, password;
        draw_register(output_mutex, 1);
        while (1) {
            std::getline(std::cin, email);
            if (email.empty()) { // 返回
                switch_to(UIPage::Start);
                return;
            }
            if (!is_email_valid(email)) {
                std::cout << "邮箱格式不合法，请重新输入：" << std::endl;
                print_input_sign();
                continue;
            }
            break;
        }
        // 发送验证码请求
        comm->handle_send_command(Action::Get_Veri_Code, email, {}, false);
        log_info("Sent veri code request");
        // 读取服务器响应
        CommandRequest resp = comm->handle_receive_command(false);
        log_info("Received veri code response");
        int action_ = resp.action();
        if (action_ != static_cast<int>(Action::Accept_Post_Code)) {
            std::cout << resp.args(0) << std::endl;
            pause();
            continue;
        }
        draw_register(output_mutex, 2);
        while (1) {
            std::getline(std::cin, veri_code);
            if (veri_code.empty() || veri_code.length() != 6) { // 假设验证码是6位
                std::cout << "验证码未完整。" << std::endl;
                print_input_sign();
                continue;
            }
            break;
        }
        // 发送验证码
        comm->handle_send_command(Action::Authentication, email, {veri_code}, false);
        // 读取服务器响应
        CommandRequest auth_resp = comm->handle_receive_command(false);
        if (auth_resp.action() != static_cast<int>(Action::Success_Auth)) {
            std::cout << "身份验证失败：" << auth_resp.args(0) << std::endl;
            pause();
            continue;
        }
        draw_register(output_mutex, 3);
        while (1) {
            std::getline(std::cin, username);
            if (!is_username_valid(username)) {
                std::cout << "请重新输入用户名：" << std::endl;
                print_input_sign();
                continue;
            }
            // 检验用户名唯一性
            std::cout << "正在检查用户名唯一性..." << std::endl;
            comm->handle_send_command(Action::Search_Person, username, {}, false);
            CommandRequest search_resp = comm->handle_receive_command(false);
            if (search_resp.action() == static_cast<int>(Action::Notify_Exist)) {
                std::cout << "用户名已存在，请重新输入：" << std::endl;
                print_input_sign();
                continue;
            }
            std::cout << "恭喜，用户名可用！" << std::endl;
            break;
        }
        draw_register(output_mutex, 4);
        while (1) {
            std::getline(std::cin, password);
            if (!is_password_valid(password)) {
                std::cout << "密码不合法。" << std::endl;
                print_input_sign();
                continue;
            }
            break;
        }
        std::string password_hash = hash_password(password);
        // 发送注册请求
        comm->handle_send_command(Action::Register, email, {username, password_hash}, false);
        log_info("Sent registration request");
        // 读取服务器响应
        CommandRequest reg_resp = comm->handle_receive_command(false);
        log_info("Received registration response");
        if (reg_resp.action() == static_cast<int>(Action::Accept_Regi)) {
            log_info("Successfully registered");
            std::cout << "注册成功！" << std::endl;
            switch_to(UIPage::Start);
            return;
        } else {
            log_info("Registration failed: {}", reg_resp.args(0));
            std::cout << reg_resp.args(0) << std::endl;
            pause();
            continue;
        }
    }
}

void WinLoop::main_init() {
    std::cout << "正在初始化数据..." << std::endl;
    std::cout << "用户ID : " << comm->cache.user_ID << std::endl;
    std::cout << "用户邮箱 : " << comm->cache.user_email << std::endl;
    // 用户email和ID写入SQLite
    comm->sqlite_con->store_user_info(comm->cache.user_ID, comm->cache.user_email, comm->cache.user_password_hash);
    // tcp连接认证, server端：handle_remember_connection
    comm->handle_send_id();
    // 发送初始化请求
    comm->handle_send_command(Action::Online_Init, comm->cache.user_ID, {}, false);
    // 拉取关系网
    std::cout << "正在拉取关系网..." << std::endl;
    comm->handle_get_relation_net();
    // 获取好友在线状态
    std::cout << "正在获取好友在线状态..." << std::endl;
    comm->handle_get_friend_status();
    // 拉取聊天记录
    std::cout << "正在拉取聊天记录..." << std::endl;
    comm->handle_get_chat_history();
    // 拉取通知和好友请求/群聊邀请/加群申请等
    // ...
    // Message, Command循环读，Data适时读, 所有通道适时写
    pool->submit([&]{
        comm->clients[0]->socket->set_nonblocking();
        while (this->running) {
            auto msg = comm->handle_receive_message();
            pool->submit([this, msg](){
                comm->handle_manage_message(msg); // 直接调用处理
            });
        }
    });
    pool->submit([&]{
        comm->clients[1]->socket->set_nonblocking();
        while (this->running) {
            auto cmd = comm->handle_receive_command();
            pool->submit([this, cmd](){
                dispatch_cmd(cmd); // 分发给不同的处理函数
            });
        }
    });
    std::cout << "数据初始化完成。" << std::endl;
    pause();
}

void WinLoop::main_loop() {
    sclear();
    draw_main(output_mutex, comm->cache.user_ID);
    handle_main_input();
}

void WinLoop::message_loop() {
    sclear();
    std::cout << "消息列表功能尚未实现。" << std::endl;
    std::cout << "按任意键返回主菜单..." << std::endl;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    pause();
    switch_to(UIPage::Main);
    return;
}

void WinLoop::contacts_loop() {
    sclear();
    draw_contacts(output_mutex, comm);
    handle_contacts_input();
}

void WinLoop::notice_loop() {
    sclear();
    if (comm->cache.notices.empty()) {
        std::cout << "没有新的通知。" << std::endl;
        pause();
        switch_to(UIPage::Contacts);
        return;
    }
    std::cout << "-$- 通知列表 -$-" << std::endl;
    std::cout << "共有" << comm->cache.notices.size() << "条通知。" << std::endl;
    while (!comm->cache.notices.empty()) {
        CommandRequest cmd;
        comm->cache.notices.pop(cmd);
        Action action = static_cast<Action>(cmd.action());
        auto time = cmd.args(0);
        std::cout << "[" << time << "] ";
        switch (action) {
            case Action::Notify: {
                std::cout << cmd.args(1) << std::endl;
                break;
            }
            case Action::Accept_FReq: {
                std::cout << cmd.sender() << "接受了你的好友请求。" << std::endl;
                break;
            }
            case Action::Refuse_FReq: {
                std::cout << cmd.sender() << "拒绝了你的好友请求。" << std::endl;
                break;
            }
            case Action::Accept_GReq: {
                std::cout << cmd.sender() << "已同意你加入群聊。" << std::endl;
                break;
            }
            case Action::Refuse_GReq: {
                std::cout << cmd.sender() << "拒绝让你加入群聊。" << std::endl;
                break;
            }
            case ::Action::Add_Friend: {
                std::cout << cmd.sender() << "已经是你的好友。" << std::endl;
                break;
            }
            case Action::Remove_Friend: {
                std::cout << cmd.sender() << "把你删掉了。" << std::endl;
                break;
            }
            case Action::Create_Group: {
                std::cout << cmd.sender() << "创建了群聊：" << cmd.args(1);
                std::cout << "，并把你拉进了群聊。" << std::endl;
                break;
            }
            case Action::Disband_Group: {
                std::cout << cmd.sender() << "解散了群聊：" << cmd.args(1) << std::endl;
                break;
            }
            case Action::Add_Admin: {
                std::cout << cmd.sender() << "把";
                if (cmd.args(2) == comm->cache.user_ID) {
                    std::cout << "你";
                } else {
                    std::cout << cmd.args(2);
                }
                std::cout << "设为群聊管理员。" << std::endl;
                break;
            }
            case Action::Remove_Admin: {
                std::cout << cmd.sender() << "把";
                if (cmd.args(2) == comm->cache.user_ID) {
                    std::cout << "你";
                } else {
                    std::cout << cmd.args(2);
                }
                std::cout << "从群聊管理员中移除。" << std::endl;
                break;
            }
        }
    }
}

void WinLoop::request_loop() {
    sclear();
    if (comm->cache.requests.empty()) {
        std::cout << "没有新的请求。" << std::endl;
        pause();
        switch_to(UIPage::Contacts);
        return;
    }
    std::cout << "共有" << comm->cache.requests.size() << "条请求。" << std::endl;
    pause();
    int count = 1;
    std::string confirm;
    auto query = [&](){
        std::cout << "是否同意？(y/n): ";
        print_input_sign();
        std::getline(std::cin, confirm);
        return confirm == "y" || confirm == "Y";
    };
    while (!comm->cache.requests.empty()) {
        sclear();
        CommandRequest cmd;
        comm->cache.requests.pop(cmd);
        Action action = static_cast<Action>(cmd.action());
        std::cout << count++ << ". [" << cmd.args(0) << "] ";
        switch (action) {
            case Action::Add_Friend_Req: {
                std::cout << cmd.sender() << "请求添加你为好友。" << std::endl;
                if (query()) {
                    comm->handle_send_command(Action::Accept_FReq, comm->cache.user_ID, {cmd.sender()}, false);
                    std::cout << "已同意好友请求。" << std::endl;
                } else {
                    comm->handle_send_command(Action::Refuse_FReq, comm->cache.user_ID, {cmd.sender()}, false);
                    std::cout << "已拒绝好友请求。" << std::endl;
                }
                break;
            }
            case Action::Accept_GReq: {
                std::cout << cmd.sender() << "请求加入群聊: ";
                std::cout << cmd.args(1) << std::endl;
                if (query()) {
                    comm->handle_send_command(Action::Accept_GReq, comm->cache.user_ID, {cmd.args(1), cmd.sender()}, false);
                    std::cout << "已同意加入群聊请求。" << std::endl;
                }
                else {
                    comm->handle_send_command(Action::Refuse_GReq, comm->cache.user_ID, {cmd.args(1), cmd.sender()}, false);
                    std::cout << "已拒绝加入群聊请求。" << std::endl;
                }
                break;
            }
            case Action::Invite_To_Group_Req: {
                std::cout << cmd.sender() << "邀请你加入群聊: ";
                std::cout << cmd.args(1) << std::endl;
                if (query()) {
                    comm->handle_send_command(Action::Join_Group_Req, comm->cache.user_ID, {cmd.args(1)}, false);
                    std::cout << "已发送加入群聊请求。" << std::endl;
                } else {
                    std::cout << "已拒绝群聊邀请。" << std::endl;
                }
                break;
            }
        }
        pause();
    }
}

void WinLoop::add_person_loop() {
    while (1) {
        sclear();
        std::cout << "请输入要添加的好友的user_ID";
        print_input_sign();
        std::string user_ID;
        std::getline(std::cin, user_ID);
        if (user_ID.empty()) { // 返回
            std::cout << "输入为空，返回联系人菜单。" << std::endl;
            pause();
            switch_to(UIPage::Contacts);
            return;
        }
        // 清空实时通知队列，确保没有旧的通知干扰
        comm->cache.real_time_notices.clear();
        // 发送搜索用户请求
        comm->handle_send_command(Action::Search_Person, comm->cache.user_ID, {user_ID}, false);
        std::cout << "正在搜索账户..." << std::endl;
        // 等待服务器响应（最多等待5秒）
        CommandRequest response;
        bool received = comm->cache.real_time_notices.wait_for_and_pop(
            response, std::chrono::seconds(5)
        );
        if (!received) {
            std::cout << "搜索超时，请检查网络连接后重试。" << std::endl;
            pause();
            continue;
        }
        // 处理响应
        Action action = static_cast<Action>(response.action());
        if (action == Action::Notify_Exist) {
            std::cout << "找到用户: " << user_ID << std::endl;
            std::cout << "是否发送好友请求？(y/n): ";
            std::string confirm;
            std::getline(std::cin, confirm);
            if (confirm == "y" || confirm == "Y") {
                // 发送添加好友请求
                comm->handle_send_command(Action::Add_Friend,
                    comm->cache.user_ID,
                    {TimeUtils::current_time_string(), user_ID}, false);
                std::cout << "好友请求已发送！" << std::endl;
            } else {
                std::cout << "已取消发送好友请求。" << std::endl;
            }
        } else if (action == Action::Notify_Not_Exist) {
            std::cout << "用户 " << user_ID << " 不存在，请检查用户ID是否正确。" << std::endl;
        } else {
            std::cout << "收到意外的响应，请重试。" << std::endl;
        }
        pause();
    }
}

void WinLoop::join_group_loop() {
    while (1) {
        sclear();
        std::cout << "请输入要加入的群组ID";
        print_input_sign();
        std::string group_ID;
        std::getline(std::cin, group_ID);
        if (group_ID.empty()) { // 返回
            std::cout << "输入为空，返回联系人菜单。" << std::endl;
            pause();
            switch_to(UIPage::Contacts);
            return;
        }
        // 清空实时通知队列，确保没有旧的通知干扰
        comm->cache.real_time_notices.clear();
        // 发送搜索群组请求
        comm->handle_send_command(Action::Search_Group, comm->cache.user_ID, {group_ID}, false);
        std::cout << "正在搜索群组..." << std::endl;
        // 等待服务器响应（最多等待5秒）
        CommandRequest response;
        bool received = comm->cache.real_time_notices.wait_for_and_pop(
            response, std::chrono::seconds(5)
        );
        if (!received) {
            std::cout << "搜索超时，请检查网络连接后重试。" << std::endl;
            pause();
            continue;
        }
        // 处理响应
        Action action = static_cast<Action>(response.action());
        if (action == Action::Notify_Exist) {
            std::cout << "找到群组: " << response.args(0) << std::endl; // group name
            std::cout << "是否加入群组？(y/n): ";
            std::string confirm;
            std::getline(std::cin, confirm);
            if (confirm == "y" || confirm == "Y") {
                // 发送加入群组请求
                comm->handle_send_command(Action::Join_Group,
                    comm->cache.user_ID,
                    { TimeUtils::current_time_string(),
                    group_ID}, false);
                std::cout << "已发送加入群组请求！" << std::endl;
            } else {
                std::cout << "已取消加入群组请求。" << std::endl;
            }
        } else if (action == Action::Notify_Not_Exist) {
            std::cout << "群组 " << group_ID << " 不存在，请检查群组ID是否正确。" << std::endl;
        } else {
            std::cout << "收到意外的响应，请重试。" << std::endl;
        }
        pause();
    }
}

void WinLoop::my_lists_loop() {
    // 暂时仅展示列表
    sclear();
    comm->print_friends();
    std::cout << std::endl << "======================"
              << std::endl << std::endl;
    comm->print_groups();
    std::cout << std::endl;
    pause();
    switch_to(UIPage::Main);
}

void WinLoop::my_loop() {
    sclear();
    std::cout << "用户ID : " << comm->cache.user_ID << std::endl;
    std::cout << "邮箱 : " << comm->cache.user_email << std::endl << std::endl << std::endl;
    std::cout << "=====================" << std::endl;
    std::cout << "开源软件仓库 : https://github.com/DecarbonizedGlucose/ChatRoom.git" << std::endl;
    pause();
    switch_to(UIPage::Main);
    return;
}

void WinLoop::log_out() {
    comm->handle_send_command(Action::Sign_Out, comm->cache.user_email, {});
}

/* ---------- 菜单选择 ---------- */

void WinLoop::handle_start_input() {
    std::string input;
    std::getline(std::cin, input);
    if (input == "1") {
        switch_to(UIPage::Login);
    } else if (input == "2") {
        switch_to(UIPage::Register);
    } else if (input == "0") {
        switch_to(UIPage::Exit);
    } else {
        std::cout << "无效的输入，请重新选择。" << std::endl;
        pause();
    }
}

void WinLoop::handle_main_input() {
    std::string input;
    std::getline(std::cin, input);
    if (input == "1") {
        switch_to(UIPage::Message);
    } else if (input == "2") {
        switch_to(UIPage::Contacts);
    } else if (input == "3") {
        switch_to(UIPage::My);
    } else if (input == "0") {
        // 退出登录
        comm->handle_send_command(Action::Sign_Out, comm->cache.user_ID, {});
        std::cout << "正在退出登录..." << std::endl;
        switch_to(UIPage::Exit);
    } else {
        std::cout << "无效的输入，请重新选择。" << std::endl;
        pause();
    }
}

void WinLoop::handle_contacts_input() {
    std::string input;
    std::getline(std::cin, input);
    if (input == "1") {
        switch_to(UIPage::Show_Notices);
    } else if (input == "2") {
        switch_to(UIPage::Manage_Requests);
    }else if (input == "3") {
        switch_to(UIPage::Add_Person);
    } else if (input == "4") {
        switch_to(UIPage::Join_Group);
    } else if (input == "5") {
        switch_to(UIPage::My_Lists);
    } else if (input == "0") {
        switch_to(UIPage::Main);
    } else {
        std::cout << "无效的输入，请重新选择。" << std::endl;
        pause();
    }
}

/* ---------- tools ---------- */

void WinLoop::sclear() {
    system("clear");
}

void WinLoop::pause() {
    std::cout << "按任意键继续...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

void WinLoop::switch_to(UIPage page) {
    current_page = page;
}

/* ---------- 渲染 ---------- */

void draw_start(std::mutex& mtx) {
    std::lock_guard<std::mutex> lock(mtx);
    std::cout << "-$-    欢迎来到聊天室    -$-" << std::endl;
    std::cout << "         " + selnum(1) + " 登录" << std::endl;
    std::cout << "         " + selnum(2) + " 注册" << std::endl;
    std::cout << "         " + selnum(0) + " 退出" << std::endl;
    print_input_sign();
}

void draw_login(std::mutex& mtx, int idx) {
    std::lock_guard<std::mutex> lock(mtx);
    switch (idx) {
        case 1: {
            std::cout << "         -$- 登    录 -$-" << std::endl;
            std::cout << "请输入邮箱/用户名和密码。" << std::endl;
            std::cout << "（直接回车返回）" << std::endl;
            std::cout << "邮箱/用户名";
            print_input_sign();
            break;
        }
        case 2: {
            std::cout << "密码";
            print_input_sign();
            break;
        }
    }
}

void draw_register(std::mutex& mtx, int idx) {
    std::lock_guard<std::mutex> lock(mtx);
    switch (idx) {
        case 1: {
            std::cout << "          -$- 注    册 -$-" << std::endl;
            std::cout << "请输入邮箱、验证码、用户名和密码。" << std::endl;
            std::cout << "（直接回车返回）" << std::endl;
            std::cout << "邮箱";
            print_input_sign();
            break;
        }
        case 2: {
            std::cout << "请注意查收验证码。验证码5分钟内有效。" << std::endl;
            std::cout << "验证码";
            print_input_sign();
            break;
        }
        case 3: {
            std::cout << "用户名只能包含字母、数字和下划线，长度在5-20位之间";
            std::cout << "用户名";
            print_input_sign();
            break;
        }
        case 4: {
            std::cout << "密码长度8-16位，至少包含一个大写字母、小写字母、\n";
            std::cout << "数字和特殊字符，且不能包含空格，在ascii 127范围内" << std::endl;
            std::cout << "密码";
            print_input_sign();
            break;
        }
    }
}

void draw_main(std::mutex& mtx, const std::string& user_ID) {
    std::lock_guard<std::mutex> lock(mtx);
    std::cout << "-$- Dear " + user_ID + ", 欢迎来到聊天室 -$-" << std::endl;
    std::cout << selnum(1) + " 消息列表" << std::endl;
    std::cout << selnum(2) + " 联系人" << std::endl;
    std::cout << selnum(3) + " 我的" << std::endl;
    std::cout << selnum(0) + " 退出登录 && 退出程序" << std::endl;
    print_input_sign();
}

void draw_contacts(std::mutex& mtx, CommManager* comm) {
    std::lock_guard<std::mutex> lock(mtx);
    std::cout << "-$- 联系人列表 -$-" << std::endl;
    std::cout << selnum(1) + " 查看通知(" << comm->cache.notices.size() << ")" << std::endl;
    std::cout << selnum(2) + " 管理请求(" << comm->cache.requests.size() << ")" << std::endl;
    std::cout << selnum(3) + " 添加好友" << std::endl;
    std::cout << selnum(4) + " 加入群聊" << std::endl;
    std::cout << selnum(5) + " 查看我的列表" << std::endl;
    std::cout << selnum(0) + " 返回主菜单" << std::endl;
    print_input_sign();
}