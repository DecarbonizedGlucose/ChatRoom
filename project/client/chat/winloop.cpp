#include "../include/winloop.hpp"
#include "../include/output.hpp"
#include "../../global/include/logging.hpp"
#include "../../global/abstract/datatypes.hpp"
#include "../../global/include/threadpool.hpp"
#include "../include/TcpClient.hpp"
#include "../include/sqlite.hpp"
#include <iostream>
#include <regex>
#include <chrono>
#include <cstdlib>
#include "../../global/include/time_utils.hpp"
#include <sstream>
#include <algorithm>

namespace {

/* ---------- 注册输入检查 ---------- */

    // 密码合规性
    bool is_password_valid(const std::string& password) {
        // 8-16位, 至少包含一个大写字母、小写字母、数字和特殊字符, 且不能包含空格, 在ascii 127范围内
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
        // 只能包含字母、数字和下划线, 长度在5-20位之间
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

/* ---------- 消息输入检查 ---------- */

    bool is_to_exit(const std::string& input) {
        const std::regex pattern(R"(^\s*/exit\s*$)");
        return std::regex_match(input, pattern);
    }

    bool is_to_upload(const std::string& input) {
        const std::regex pattern(R"(^\s*/up\s+(/[^\s]+)\s*$)");
        return std::regex_match(input, pattern);
    }

    bool is_to_download(const std::string& input) {
        const std::regex pattern(R"(^\s*/down\s+([a-zA-Z0-9_-]+)\s+(\S+)\s*$)");
        return std::regex_match(input, pattern);
    }

}

WinLoop::WinLoop(CommManager* comm, thread_pool* pool)
    : current_page(UIPage::Start), comm(comm), pool(pool) {}

WinLoop::~WinLoop() {
    running = false;
}

void WinLoop::dispatch_cmd(const CommandRequest& cmd) {
    Action action = static_cast<Action>(cmd.action());
    std::string sender = cmd.sender();
    auto args = cmd.args();
    switch (action) {
        case Action::Add_Friend_Req: {        // 处理好友请求
            comm->print_request_notice();
            comm->cache.requests.push(cmd);
            return;
        }
        case Action::Join_Group_Req: {        // 处理加入群组请求
            comm->print_request_notice();
            comm->cache.requests.push(cmd);
            return;
        }
        case Action::Invite_To_Group_Req: {   // 处理邀请加入群组请求
            comm->print_request_notice();
            comm->cache.requests.push(cmd);
            return;
        }
        case Action::Notify_Exist: {          // 通知用户/用户名/群组存在
            comm->cache.real_time_notices.push(cmd);
            return;
        }
        case Action::Notify_Not_Exist: {      // 通知用户/用户名/群组不存在
            comm->cache.real_time_notices.push(cmd);
            return;
        }
        case Action::Give_Group_ID: {
            comm->cache.real_time_notices.push(cmd);
            return;
        }
        case Action::Friend_Online: {         // 好友上线
            if (args[0] != comm->cache.user_ID) {
                comm->cache.friend_list[args[0]].online = true;
            } else {
                log_debug("Ignoring Friend_Online for self: {}", args[0]);
            }
            return;
        }
        case Action::Friend_Offline: {        // 好友下线
            if (args[0] != comm->cache.user_ID) {
                comm->cache.friend_list[args[0]].online = false;
            } else {
                log_debug("Ignoring Friend_Offline for self: {}", args[0]);
            }
            return;
        }
        case Action::Accept_FReq: {           // 同意好友请求
            comm->print_notice_notice();
            auto friend_ID = cmd.sender();
            comm->cache.notices.push(cmd);
            if (comm->cache.friend_list.find(friend_ID) == comm->cache.friend_list.end()) {
                /**
                 * 有可能上线前就同意了，那样不用再存一次，因为拉取了。
                 * 但是，目前没加这个功能，所以只会在上线实时收到通知
                 * 可扩展性处理，这里加个判断
                 */
                comm->handle_add_friend(friend_ID);
            }
            return;
        }
        case Action::Refuse_FReq: {           // 拒绝好友请求
            comm->print_notice_notice();
            comm->cache.notices.push(cmd);
            return;
        }
        case Action::Remove_Friend: {         // 被删除了
            comm->print_notice_notice();
            comm->cache.notices.push(cmd);
            comm->handle_remove_friend(sender);
            return;
        }
        case Action::Accept_GReq: {           // 同意加群申请
            // 加入群组的请求
            auto& group_ID = cmd.args(1);
            if (cmd.args(2) == comm->cache.user_ID
                || comm->cache.group_list[group_ID].is_user_admin) {
                comm->print_notice_notice();
                comm->cache.notices.push(cmd); // 管理员、被同意的自己
            }
            // 这里面要拉取群成员名单
            // 函数内有是否已经在的判断了
            if (cmd.args(2) == comm->cache.user_ID) {
                comm->handle_join_group(
                    cmd.args(1), cmd.args(3), std::stoi(cmd.args(4)), cmd.args(5)
                );
            } else {
                comm->handle_add_person(cmd.args(1), cmd.args(2));
            }
            return;
        }
        case Action::Refuse_GReq: {           // 拒绝加群申请
            comm->print_notice_notice();
            // 拒绝加入群组的请求
            comm->cache.notices.push(cmd);
            return;
        }
        case Action::Leave_Group: {           // 自己跑了
            // 在本地删掉这个人。如果是管理员，还要通知
            comm->handle_remove_person(cmd.args(1), cmd.sender());
            if (comm->cache.group_list[cmd.args(1)].is_user_admin) {
                comm->print_notice_notice();
                comm->cache.notices.push(cmd);
            }
            comm->handle_remove_person(cmd.args(1), cmd.sender());
            return;
        }
        case Action::Disband_Group: {         // 解散群组
            comm->print_notice_notice();
            comm->cache.notices.push(cmd);
            comm->handle_disband_group(cmd.args(1));
            return;
        }
        case Action::Remove_From_Group: {     // 被踢
            // 可能是自己，也可能是别人
            if (cmd.args(2) == comm->cache.user_ID) {
                comm->print_notice_notice();
                comm->cache.notices.push(cmd);
                comm->handle_leave_group(cmd.args(1));
            } else if (comm->cache.group_list[cmd.args(1)].is_user_admin) {
                comm->print_notice_notice();
                comm->cache.notices.push(cmd);
                comm->handle_remove_person(cmd.args(1), cmd.args(2));
            } else {
                comm->handle_remove_person(cmd.args(1), cmd.args(2));
            }
            return;
        }
        case Action::Add_Admin: {             // 新增管理员
            if (comm->cache.group_list[cmd.args(1)].is_user_admin || cmd.args(2) == comm->cache.user_ID) {
                comm->print_notice_notice();
                comm->cache.notices.push(cmd);
            }
            comm->handle_add_admin(cmd.args(1), cmd.args(2));
            return;
        }
        case Action::Remove_Admin: {          // 移除管理员
            if (comm->cache.group_list[cmd.args(1)].is_user_admin) {
                comm->print_notice_notice();
                comm->cache.notices.push(cmd);
            }
            comm->handle_remove_admin(cmd.args(1), cmd.args(2));
            return;
        }
        case Action::Accept_File: {
            comm->cache.real_time_notices.push(cmd);
            break;
        }
        case Action::Deny_File: {
            comm->cache.real_time_notices.push(cmd);
            return;
        }
        case Action::Accept_File_Req: {
            comm->cache.real_time_notices.push(cmd);
            return;
        }
        case Action::Deny_File_Req: {
            comm->cache.real_time_notices.push(cmd);
            return;
        }
        case Action::Success: {               // 竞争处理群聊事务成功
            comm->cache.real_time_notices.push(cmd);
            return;
        }
        case Action::Managed: {               // 已经被处理
            comm->cache.real_time_notices.push(cmd);
            return;
        }
        case Action::HEARTBEAT: {             // 心跳检测
            comm->handle_reply_heartbeat();
            return;
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
            case UIPage::Chat:
                chat_loop();
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
            case UIPage::My_Lists:
                my_lists_loop();
                break;
            case UIPage::My:
                my_loop();
                break;
            case UIPage::Exit:
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

void WinLoop::login_loop() { // 改进用email/ID均可登录, 并返回另一个
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
            system("stty -echo");
            std::getline(std::cin, password);
            system("stty echo");
            std::cout << std::endl;
            if (user.empty() || password.empty()) {
                std::cout << "不能输入空字符串" << std::endl;
                print_input_sign();
                continue;
            }
            break;
        }
        auto password_hash = hash_password(password);

        log_info("Attempting login for user: {}", user);

        // 传给服务器
        try {
            comm->handle_send_command(Action::Sign_In, user, {password_hash}, false);
            log_info("Login command sent successfully");

            // 读取服务器响应
            std::cout << "正在登录, 请稍候..." << std::endl;
            CommandRequest resp = comm->handle_receive_command(false);
            log_info("Received response from server: action={}", resp.action());

            if (resp.action() == static_cast<int>(Action::Accept_Login)) {
                std::cout << "登录成功！" << std::endl;
                log_info("Login successful for user: {}", user);
                if (is_email_valid(user)) {
                    comm->cache.user_email = user;
                    comm->cache.user_ID = resp.args(0);
                } else {
                    comm->cache.user_ID = user;
                    comm->cache.user_email = resp.args(0);
                }
                comm->cache.user_password_hash = password_hash;
                comm->online = true;
                main_init();
                switch_to(UIPage::Main);
                return;
            } else { // Refused
                std::cout << "登录失败：" << resp.args(0) << std::endl;
                log_error("Login failed for user {}: {}", user, resp.args(0));
                pause();
                continue;
            }
        } catch (const std::exception& e) {
            std::cout << "网络错误, 请检查连接后重试" << std::endl;
            log_error("Network error during login: {}", e.what());
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
                std::cout << "邮箱格式不合法, 请重新输入：" << std::endl;
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
            comm->handle_send_command(Action::Search_Person, "", {username}, false);
            CommandRequest search_resp = comm->handle_receive_command(false);
            if (search_resp.action() == static_cast<int>(Action::Notify_Exist)) {
                std::cout << "用户名已存在, 请重新输入：" << std::endl;
                print_input_sign();
                continue;
            }
            std::cout << "恭喜, 用户名可用！" << std::endl;
            break;
        }
        draw_register(output_mutex, 4);
        while (1) {
            system("stty -echo");
            std::getline(std::cin, password);
            system("stty echo");
            std::cout << std::endl;
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
            pause();
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
    // std::cout << "用户ID : " << comm->cache.user_ID << std::endl;
    // std::cout << "用户邮箱 : " << comm->cache.user_email << std::endl;
    // 用户email和ID写入SQLite
    comm->sqlite_con->store_user_info(comm->cache.user_ID, comm->cache.user_email, comm->cache.user_password_hash);
    // tcp连接认证, server端：handle_remember_connection
    comm->handle_send_id();
    // 发送初始化请求
    comm->handle_send_command(Action::Online_Init, comm->cache.user_ID, {}, false);

    // Message, Command循环读, Data适时读, 所有通道适时写
    pool->submit([&]{
        comm->clients[0]->socket->set_nonblocking();
        while (comm->online) {
            auto msg = comm->handle_receive_message();
            pool->submit([this, msg](){
                comm->handle_manage_message(msg); // 直接调用处理
            });
        }
    });
    pool->submit([&]{
        comm->clients[1]->socket->set_nonblocking();
        while (comm->online) {
            auto cmd = comm->handle_receive_command();
            pool->submit([this, cmd](){
                log_debug("Received command: action={}, sender={}",
                    static_cast<int>(cmd.action()), cmd.sender());
                dispatch_cmd(cmd); // 分发给不同的处理函数
            });
        }
    });

    // 拉取关系网
    std::cout << "正在拉取关系网..." << std::endl;
    comm->handle_get_relation_net();
    // 获取好友在线状态
    std::cout << "正在获取好友在线状态..." << std::endl;
    comm->handle_get_friend_status();
    // 拉取聊天记录
    std::cout << "正在拉取聊天记录..." << std::endl;
    comm->handle_get_chat_history();
    // 从SQLite加载历史聊天记录
    std::cout << "正在加载历史聊天记录..." << std::endl;
    comm->load_conversation_history();

    std::cout << "数据初始化完成。" << std::endl;
    pause();
}

void WinLoop::main_loop() {
    sclear();
    draw_main(output_mutex, comm->cache.user_ID);
    handle_main_input();
}

void WinLoop::message_loop() {
    while (true) {
        // 更新会话列表
        comm->update_conversation_list();
        auto conv_list = comm->cache.get_sorted_conversations();

        sclear();

        // 显示标题
        std::cout << "-$- 消息列表 -$-" << std::endl;

        // 显示返回选项
        std::cout << selnum(0) << " 返回主菜单" << std::endl;

        // 显示会话列表
        display_conversation_list(conv_list);

        print_input_sign();

        // 处理用户输入
        std::string input;
        std::getline(std::cin, input);

        // 去除可能的回车符
        if (!input.empty() && input.back() == '\r') {
            input.pop_back();
        }

        if (input == "0") {
            // 返回主菜单
            switch_to(UIPage::Main);
            return;
        } else {
            // 尝试解析为数字
            try {
                int choice = std::stoi(input);
                if (choice >= 1 && choice <= (int)conv_list.size()) {
                    // 进入选中的会话
                    std::string conversation_id = conv_list[choice - 1].first;
                    comm->cache.current_conversation_id = conversation_id;

                    // 清零未读计数
                    comm->cache.conversations[conversation_id].unread_count = 0;

                    switch_to(UIPage::Chat);
                    return;
                } else {
                    std::cout << "无效的选择, 请重新输入。" << std::endl;
                    pause();
                }
            } catch (const std::exception&) {
                std::cout << "无效的输入, 请输入数字。" << std::endl;
                pause();
            }
        }
    }
}

void WinLoop::chat_loop() {
    sclear();
    auto conv_id = comm->cache.current_conversation_id;

    if (comm->cache.conversations[conv_id].is_group) {
        if (comm->cache.group_members.find(conv_id) == comm->cache.group_members.end()) {
            auto mems = comm->sqlite_con->get_cached_group_members_with_admin_status(conv_id);
            for (auto& [member, is_admin] : mems) {
                comm->cache.group_members[conv_id][member] = is_admin;
            }
        }
    }
    auto history = comm->get_conversation_messages(conv_id);
    for (auto msg : history) {
        render_message(msg);
    }
    std::string input;
    bool chatting = true;
    pool->submit([&]{
        while (chatting) {
            ChatMessage msg;
            bool got = comm->cache.new_messages.wait_for_and_pop(msg, std::chrono::milliseconds(100));
            if (!got) {
                // 定期检查当前会话是否仍然有效
                if (comm->cache.conversations.find(conv_id) == comm->cache.conversations.end()) {
                    // std::lock_guard<std::mutex> lock(output_mutex);
                    // std::cout << '\r' << std::string(50, ' ') << '\r'; // 清除当前行
                    // std::cout << "\n[系统消息] 会话已不存在，即将返回消息列表。\n" << std::endl;
                    chatting = false; // 设置标志以退出聊天循环
                }
                continue; // 这样能及时跳出，避免线程池被占用
            }
            if ((msg.receiver() == conv_id || msg.sender() == conv_id)) {
                std::lock_guard<std::mutex> lock(output_mutex);
                std::cout << '\r' << std::string(50, ' ') << '\r'; // 清除当前行
                render_message(msg);
                print_header();
                print_input_sign();
            }
        }
    });
    while (chatting) {
        print_header();
        print_input_sign();
        std::getline(std::cin, input);
        if (input.empty()) continue;
        if (is_to_exit(input)) {
            // 退出聊天
            chatting = false;
            break;
        }
        if (is_to_upload(input)) {
            // 上传文件
            std::istringstream iss(input);
            std::string cmd, path;
            iss >> cmd >> path;
            auto file = std::make_shared<ClientFile>(path);
            if (file->status == FileStatus::FAILED) {
                std::cout << "[系统消息] 文件不存在或者是个目录。" << std::endl << std::endl;
                continue;
            }
            // 发送上传请求
            comm->handle_send_command(Action::Upload_File,
                comm->cache.user_ID,
                {file->file_hash, std::to_string(file->file_size)});
            CommandRequest resp;
            bool got = comm->cache.real_time_notices.wait_for_and_pop(resp, std::chrono::seconds(5));
            if (!got) {
                std::cout << "\r[系统消息] 上传请求超时，请稍后重试。" << std::endl << std::endl;
                continue;
            }
            if (resp.action() != static_cast<int>(Action::Accept_File)) {
                if (resp.args_size() > 1 && resp.args(1) == "1") {
                    std::cout << "\r[系统消息] 服务器已有该文件，无需重复上传。" << std::endl;
                    // Deny_File with sendable="1": args(0)=file_hash, args(1)="1", args(2)=file_id
                    if (resp.args_size() > 2) {
                        comm->send_file_message(
                            conv_id,
                            comm->cache.conversations[conv_id].is_group,
                            file,
                            resp.args(2));  // file_id
                    } else {
                        std::cout << "\r[系统消息] 服务器响应格式错误。" << std::endl;
                    }
                    continue;
                } else {
                    std::cout << "\r[系统消息] 服务器拒绝上传。" << std::endl << std::endl;
                    continue;
                }
            }
            std::cout << "\r[系统消息] 上传请求已发送，正在上传文件" << std::endl << std::endl;
            comm->file_manager->upload_file(path);
            comm->send_file_message(
                conv_id,
                comm->cache.conversations[conv_id].is_group,
                file,
                resp.args(1));
        } else if (is_to_download(input)) {
            // 下载文件
            std::regex pattern(R"(^\s*/down\s+([a-zA-Z0-9_-]+)\s+(\S+)\s*$)");
            std::smatch matches;
            if (std::regex_match(input, matches, pattern)) {
                std::string file_id = matches[1].str();
                std::string file_name = matches[2].str();
                comm->handle_send_command(Action::Download_File,
                    comm->cache.user_ID,
                    {file_id});
                std::cout << "\r[系统消息] 正在发送下载请求，请稍候..." << std::endl;
                CommandRequest resp;
                bool got = comm->cache.real_time_notices.wait_for_and_pop(resp, std::chrono::seconds(5));
                if (!got) {
                    std::cout << "\r[系统消息] 下载请求超时，请稍后重试。" << std::endl << std::endl;
                    continue;
                }
                if (resp.action() != static_cast<int>(Action::Accept_File_Req)) {
                    std::cout << "\r[系统消息] 文件不存在。" << std::endl << std::endl;
                    continue;
                }
                std::cout << "\r[系统消息] 下载请求已发送，正在下载文件..." << std::endl;
                std::string download_path = std::string(std::getenv("HOME")) + "/Downloads/" + file_name;
                comm->file_manager->download_file( // 包含了线程池操作
                    file_name,
                    download_path,
                    resp.args(1),
                    std::stol(resp.args(2))
                );
            }
        } else {
            // 普通文本消息
            comm->send_text_message(conv_id,
                comm->cache.conversations[conv_id].is_group,
                input);
            std::cout << std::endl;
        }
    }
    switch_to(UIPage::Message);
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

    // 使用非破坏性方式获取所有通知的副本
    std::vector<CommandRequest> notices_copy = comm->cache.notices.copy_all();

    // 显示所有通知
    for (const auto& cmd : notices_copy) {
        Action action = static_cast<Action>(cmd.action());
        if (cmd.args_size() > 0) {
            auto time = cmd.args(0);
            std::cout << '[' << time << "] ";
        }
        switch (action) {
            case Action::Accept_FReq: {
                std::cout << cmd.sender() << "接受了你的好友请求。" << std::endl;
                break;
            }
            case Action::Refuse_FReq: {
                std::cout << cmd.sender() << "拒绝了你的好友请求。" << std::endl;
                break;
            }
            case Action::Accept_GReq: {
                auto& group_ID = cmd.args(1);
                auto& group_name = cmd.args(3);
                auto& user_ID = cmd.args(2);
                std::cout << cmd.sender() << "已同意"
                    + (user_ID == comm->cache.user_ID ? "你" : user_ID)
                    + "加入群聊" + group_name + '('
                    + group_ID + ')' << std::endl;
                break;
            }
            case Action::Refuse_GReq: {
                auto& group_ID = cmd.args(1);
                auto& user_ID = cmd.args(2);
                std::cout << cmd.sender() << "拒绝让"
                    + (user_ID == comm->cache.user_ID ? "你" : user_ID)
                    + "加入群聊"
                    + group_ID +"。" << std::endl;
                break;
            }
            case Action::Remove_Friend: {
                std::cout << cmd.sender() << "把你删掉了。" << std::endl;
                break;
            }
            case Action::Create_Group: {
                std::cout << cmd.sender() << "创建了群聊：" << cmd.args(1);
                std::cout << ", 并把你拉进了群聊。" << std::endl;
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
                std::cout << "设为群聊" <<
                cmd.args(1) <<"的管理员。" << std::endl;
                break;
            }
            case Action::Remove_Admin: {
                std::cout << cmd.sender() << "把";
                if (cmd.args(2) == comm->cache.user_ID) {
                    std::cout << "你";
                } else {
                    std::cout << cmd.args(2);
                }
                std::cout << "从群聊" <<
                cmd.args(1) << "的管理员列表中移除。" << std::endl;
                break;
            }
            case Action::Remove_From_Group: {
                std::cout << cmd.sender() << "把";
                if (cmd.args(2) == comm->cache.user_ID) {
                    std::cout << "你";
                } else {
                    std::cout << cmd.args(2);
                }
                std::cout << "从群聊" << cmd.args(1) << "中移除。" << std::endl;
                break;
            }
            case Action::Leave_Group: {
                std::cout << cmd.sender() << "离开了群聊：" << cmd.args(1) << std::endl;
                break;
            }
        }
    }

    std::cout << std::endl << "是否清空所有通知？(y/n): ";
    std::string confirm;
    std::getline(std::cin, confirm);
    if (confirm == "y" || confirm == "Y") {
        // 用户确认后才清空通知
        comm->cache.notices.clear();
        std::cout << "所有通知已清空。" << std::endl;
    }

    pause();
    switch_to(UIPage::Contacts);
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
        std::cout << "是否同意？(y/n): " << std::endl;
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
                    comm->handle_send_command(Action::Accept_FReq,
                        comm->cache.user_ID,
                        {TimeUtils::current_time_string(),
                        cmd.sender()}, false);
                    std::cout << "已同意好友请求。" << std::endl;
                    // 写入本地
                    comm->handle_add_friend(cmd.sender());
                } else {
                    comm->handle_send_command(Action::Refuse_FReq,
                        comm->cache.user_ID,
                        {TimeUtils::current_time_string(),
                        cmd.sender()}, false);
                    std::cout << "已拒绝好友请求。" << std::endl;
                }
                break;
            }
            case Action::Join_Group_Req: {
                std::cout << cmd.sender() << "请求加入群聊: ";
                std::cout << cmd.args(1) << std::endl;
                auto cmd_id = cmd.args(2);
                if (query()) {
                    comm->handle_send_command(Action::Accept_GReq,
                        comm->cache.user_ID,
                        {TimeUtils::current_time_string(),
                        cmd_id}, false);
                    // 接收处理情况
                    CommandRequest resp;
                    bool got = comm->cache.real_time_notices.wait_for_and_pop(
                        resp, std::chrono::seconds(5));
                    if (!got) {
                        std::cout << "处理超时，请稍后重试。" << std::endl;
                        break;
                    }
                    if (resp.action() != static_cast<int>(Action::Success)) {
                        std::cout << "该请求已被其他管理员处理。" << std::endl;
                        break;
                    }
                    std::cout << "已同意加入群聊请求。" << std::endl;
                    // 写入本地
                    comm->handle_add_person(cmd.args(1), cmd.sender());
                }
                else {
                    comm->handle_send_command(Action::Refuse_GReq,
                        comm->cache.user_ID,
                        {TimeUtils::current_time_string(),
                        cmd_id}, false);
                    // 接收处理情况
                    CommandRequest resp;
                    bool got = comm->cache.real_time_notices.wait_for_and_pop(
                        resp, std::chrono::seconds(5));
                    if (!got) {
                        std::cout << "处理超时，请稍后重试。" << std::endl;
                        break;
                    }
                    if (resp.action() != static_cast<int>(Action::Success)) {
                        std::cout << "该请求已被其他管理员处理。" << std::endl;
                        break;
                    }
                    std::cout << "已拒绝加入群聊请求。" << std::endl;
                }
                break;
            }
            case Action::Invite_To_Group_Req: {
                std::cout << cmd.sender() << "邀请你加入群聊: ";
                std::cout << cmd.args(2) << '(' << cmd.args(1) << ')' << std::endl;
                if (query()) {
                    comm->handle_send_command(Action::Join_Group_Req,
                        comm->cache.user_ID,
                        {TimeUtils::current_time_string(),
                        cmd.args(1)}, false);
                    std::cout << "已发送加入群聊请求。" << std::endl;
                } else {
                    std::cout << "已取消加入群聊。" << std::endl;
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
        std::cout << "请输入要添加的好友的user_ID" << std::endl;
        print_input_sign();
        std::string user_ID;
        std::getline(std::cin, user_ID);
        if (user_ID.empty()) { // 返回
            std::cout << "输入为空, 返回。" << std::endl;
            pause();
            return;
        }
        // 清空实时通知队列, 确保没有旧的通知干扰
        //comm->cache.real_time_notices.clear();
        // 发送搜索用户请求
        comm->handle_send_command(Action::Search_Person, comm->cache.user_ID, {user_ID}, false);
        std::cout << "正在搜索账户..." << std::endl;
        // 等待服务器响应（最多等待5秒）
        CommandRequest response;
        bool received = comm->cache.real_time_notices.wait_for_and_pop(
            response, std::chrono::seconds(5)
        );
        if (!received) {
            std::cout << "搜索超时, 请检查网络连接后重试。" << std::endl;
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
                comm->handle_send_command(Action::Add_Friend_Req,
                    comm->cache.user_ID,
                    {TimeUtils::current_time_string(), user_ID}, false);
                std::cout << "好友请求已发送！" << std::endl;
            } else {
                std::cout << "已取消发送好友请求。" << std::endl;
            }
        } else if (action == Action::Notify_Not_Exist) {
            std::cout << "用户 " << user_ID << " 不存在, 请检查用户ID是否正确。" << std::endl;
        } else {
            std::cout << "收到意外的响应, 请重试。" << std::endl;
        }
        pause();
    }
}

void WinLoop::join_group_loop() {
    while (1) {
        sclear();
        std::cout << "请输入要加入的群组ID" << std::endl;
        print_input_sign();
        std::string group_ID;
        std::getline(std::cin, group_ID);
        if (group_ID.empty()) { // 返回
            std::cout << "输入为空, 返回。" << std::endl;
            pause();
            return;
        }
        // 清空实时通知队列, 确保没有旧的通知干扰
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
            std::cout << "搜索超时, 请检查网络连接后重试。" << std::endl;
            pause();
            continue;
        }
        // 处理响应
        Action action = static_cast<Action>(response.action());
        if (action == Action::Notify_Exist) {
            std::cout << "找到群组: " << response.args(1)
            << '(' << response.args(0) << ')' << std::endl; // group name
            std::cout << "是否加入群组？(y/n): ";
            std::string confirm;
            std::getline(std::cin, confirm);
            if (confirm == "y" || confirm == "Y") {
                // 发送加入群组请求
                comm->handle_send_command(Action::Join_Group_Req,
                    comm->cache.user_ID,
                    { TimeUtils::current_time_string(),
                    group_ID}, false);
                std::cout << "已发送加入群组请求！" << std::endl;
            } else {
                std::cout << "已取消加入群组请求。" << std::endl;
            }
        } else if (action == Action::Notify_Not_Exist) {
            std::cout << "群组 " << group_ID << " 不存在, 请检查群组ID是否正确。" << std::endl;
        } else {
            std::cout << "收到意外的响应, 请重试。" << std::endl;
        }
        pause();
    }
}

void WinLoop::my_lists_loop() {
    std::string input;
    std::string user_ID, group_ID, group_name;
    while (1) {
        sclear();
        draw_my_lists(output_mutex, comm);
        std::getline(std::cin, input);
        if (input == "1") {
            // 查看好友列表
            comm->print_friends();
        } else if (input == "2") {
            // 查看群组列表
            comm->print_groups();
        } else if (input == "3") {
            // 添加好友
            add_person_loop();
            continue;
        } else if (input == "4") {
            // 加入群聊
            join_group_loop();
            continue;
        } else if (input == "5") {
            // 删除好友
            std::cout << "请输入要删除的好友ID：" << std::endl;
            print_input_sign();
            std::getline(std::cin, user_ID);
            if (user_ID.empty()) {
                std::cout << "输入为空." << std::endl;
            } else if (comm->cache.friend_list.find(user_ID) == comm->cache.friend_list.end()) {
                std::cout << "好友不存在, 请检查ID是否正确。" << std::endl;
            } else {
                comm->handle_send_command(Action::Remove_Friend,
                    comm->cache.user_ID,
                    {TimeUtils::current_time_string(), user_ID}, false);
                std::cout << "已发送删除好友请求。" << std::endl;
                // 从本地缓存中删除
                comm->handle_remove_friend(user_ID);
            }
        } else if (input == "6") {
            // 屏蔽好友
            std::cout << "请输入要屏蔽的好友ID：" << std::endl;
            print_input_sign();
            std::getline(std::cin, user_ID);
            if (user_ID.empty()) {
                std::cout << "输入为空." << std::endl;
            } else if (user_ID == comm->cache.user_ID) {
                std::cout << "你不能屏蔽自己。" << std::endl;
            } else if (comm->cache.friend_list.find(user_ID) == comm->cache.friend_list.end()) {
                std::cout << "好友不存在, 请检查ID是否正确。" << std::endl;
            } else if (comm->cache.friend_list[user_ID].blocked) {
                std::cout << "好友已被屏蔽。" << std::endl;
            } else {
                std::cout << "正在屏蔽好友 " << user_ID << "..." << std::endl;
                // 发送屏蔽好友请求
                comm->handle_send_command(Action::Block_Friend,
                    comm->cache.user_ID,
                    {user_ID}, false);
                std::cout << "已发送屏蔽好友请求。" << std::endl;
                comm->handle_block_friend(user_ID);
            }
        } else if (input == "7") {
            // 取消屏蔽好友
            std::cout << "请输入要取消屏蔽的好友ID：" << std::endl;
            print_input_sign();
            std::getline(std::cin, user_ID);
            if (user_ID.empty()) {
                std::cout << "输入为空." << std::endl;
            } else if (user_ID == comm->cache.user_ID) {
                std::cout << "你不能取消屏蔽自己。" << std::endl;
            } else if (comm->cache.friend_list.find(user_ID) == comm->cache.friend_list.end()) {
                std::cout << "好友不存在, 请检查ID是否正确。" << std::endl;
            } else if (!comm->cache.friend_list[user_ID].blocked) {
                std::cout << "好友未被屏蔽。" << std::endl;
            } else {
                std::cout << "正在取消屏蔽好友 " << user_ID << "..." << std::endl;
                // 发送取消屏蔽好友请求
                comm->handle_send_command(Action::Unblock_Friend,
                    comm->cache.user_ID,
                    {user_ID}, false);
                std::cout << "已发送取消屏蔽好友请求。" << std::endl;
                comm->handle_unblock_friend(user_ID);
            }
        } else if (input == "8") {
            // 退出群聊
            std::cout << "请输入要退出的群组ID：" << std::endl;
            print_input_sign();
            std::getline(std::cin, group_ID);
            if (group_ID.empty()) {
                std::cout << "输入为空." << std::endl;
            } else if (comm->cache.group_list.find(group_ID) == comm->cache.group_list.end()) {
                std::cout << "群组不存在, 请检查ID是否正确。" << std::endl;
            } else if (comm->cache.group_list[group_ID].owner_ID == comm->cache.user_ID) {
                std::cout << "群主请使用/disband。" << std::endl;
            } else {
                std::cout << "正在退出群组 " << group_ID << "..." << std::endl;
                // 发送退出群组请求
                comm->handle_send_command(Action::Leave_Group,
                    comm->cache.user_ID,
                    {TimeUtils::current_time_string(), group_ID}, false);
                std::cout << "已发送退出群组请求。" << std::endl;
                comm->handle_leave_group(group_ID);
            }
        } else if (input == "9") {
            // 解散群聊
            std::cout << "请输入要解散的群组ID：" << std::endl;
            print_input_sign();
            std::getline(std::cin, group_ID);
            if (group_ID.empty()) {
                std::cout << "输入为空." << std::endl;
            } else if (comm->cache.group_list.find(group_ID) == comm->cache.group_list.end()) {
                std::cout << "群组不存在, 请检查ID是否正确。" << std::endl;
            } else {
                std::cout << "正在解散群组 " << group_ID << "..." << std::endl;
                // 发送解散群组请求
                comm->handle_send_command(Action::Disband_Group,
                    comm->cache.user_ID,
                    {TimeUtils::current_time_string(), group_ID}, false);
                std::cout << "已发送解散群组请求。" << std::endl;
                comm->handle_disband_group(group_ID);
            }
        } else if (input == "10") {
            // 取消管理员
            std::cout << "请输入要取消管理员的群组ID：" << std::endl;
            print_input_sign();
            std::getline(std::cin, group_ID);
            if (group_ID.empty()) {
                std::cout << "群组ID不能为空" << std::endl;
            } else if (comm->cache.group_list.find(group_ID) == comm->cache.group_list.end()) {
                std::cout << "群组不存在, 请检查ID是否正确。" << std::endl;
            } else if (comm->cache.group_members.find(group_ID) == comm->cache.group_members.end()
                        || comm->cache.group_members[group_ID].empty()) {
                std::cout << "群组成员列表未正确加载, 请稍后再试。" << std::endl;
            } else if (comm->cache.group_list[group_ID].owner_ID != comm->cache.user_ID) {
                std::cout << "你不是这个群的群主。" << std::endl;
            } else {
                std::cout << "请输入要取消管理员的用户ID：" << std::endl;
                print_input_sign();
                std::getline(std::cin, user_ID);
                if (user_ID.empty()) {
                    std::cout << "管理员ID不能为空" << std::endl;
                } else if (user_ID == comm->cache.user_ID) {
                    std::cout << "你不能移除自己的管理员身份。" << std::endl;
                } else if (comm->cache.group_members[group_ID].find(user_ID) == comm->cache.group_members[group_ID].end()) {
                    std::cout << "该用户不是群组成员。" << std::endl;
                } else if (!comm->cache.group_members[group_ID][user_ID]) {
                    std::cout << "该用户不是群组管理员。" << std::endl;
                } else {
                    std::cout << "正在移除群组 " << group_ID << " 的管理员 " << user_ID << "..." << std::endl;
                    // 发送移除管理员请求
                    comm->handle_send_command(Action::Remove_Admin,
                        comm->cache.user_ID,
                        {TimeUtils::current_time_string(), group_ID, user_ID}, false);
                    std::cout << "已发送移除管理员请求。" << std::endl;
                    comm->handle_remove_admin(group_ID, user_ID);
                }
            }
        } else if (input == "11") {
            // 添加管理员
            std::cout << "请输入要添加管理员的群组ID：" << std::endl;
            print_input_sign();
            std::getline(std::cin, group_ID);
            if (group_ID.empty()) {
                std::cout << "群组ID不能为空" << std::endl;
            } else if (comm->cache.group_list.find(group_ID) == comm->cache.group_list.end()) {
                std::cout << "群组不存在, 请检查ID是否正确。" << std::endl;
            } else if (comm->cache.group_members.find(group_ID) == comm->cache.group_members.end()
                        || comm->cache.group_members[group_ID].empty()) {
                std::cout << "群组成员列表未正确加载, 请稍后再试。" << std::endl;
            } else if (comm->cache.group_list[group_ID].owner_ID != comm->cache.user_ID) {
                std::cout << "你不是这个群的群主。" << std::endl;
            } else {
                std::cout << "请输入要添加为管理员的用户ID：" << std::endl;
                print_input_sign();
                std::getline(std::cin, user_ID);
                if (user_ID.empty()) {
                    std::cout << "管理员ID不能为空" << std::endl;
                } else if (user_ID == comm->cache.user_ID) {
                    std::cout << "你本来就是管理员。" << std::endl;
                } else if (comm->cache.group_members[group_ID].find(user_ID) == comm->cache.group_members[group_ID].end()) {
                    std::cout << "该用户不是群组成员, 请先添加为群组成员。" << std::endl;
                } else if (comm->cache.group_members[group_ID][user_ID]) {
                    std::cout << "该用户已经是群组管理员。" << std::endl;
                } else {
                    std::cout << "正在添加群组 " << group_ID << " 的管理员 " << user_ID << "..." << std::endl;
                    // 发送添加管理员请求
                    comm->handle_send_command(Action::Add_Admin,
                        comm->cache.user_ID,
                        {TimeUtils::current_time_string(), group_ID, user_ID}, false);
                    std::cout << "已发送添加管理员请求。" << std::endl;
                    comm->handle_add_admin(group_ID, user_ID);
                }
            }
        } else if (input == "12") {
            // 创建群聊
            std::cout << "请输入新群组的名称：" << std::endl;
            print_input_sign();
            std::getline(std::cin, group_name);
            if (group_name.empty()) {
                std::cout << "输入为空。" << std::endl;
            } else if (group_name.find("Group_") == 0) {
                std::cout << "群组名称不能以 'Group_' 开头。" << std::endl;
            } else {
                std::cout << "正在创建群组 " << group_name << "..." << std::endl;
                // 发送创建群组请求
                comm->handle_send_command(Action::Create_Group,
                    comm->cache.user_ID,
                    {TimeUtils::current_time_string(), group_name}, false);
                std::cout << "已发送创建群组请求。" << std::endl;
                // 等待服务器响应（最多等待5秒）
                CommandRequest response;
                bool received = comm->cache.real_time_notices.wait_for_and_pop(
                    response, std::chrono::seconds(5)
                );
                if (!received) {
                    std::cout << "创建群组请求超时, 请检查网络连接后重试。" << std::endl;
                }
                // 处理响应
                Action action = static_cast<Action>(response.action());
                if (action == Action::Give_Group_ID && !response.args(0).empty()) {
                    group_ID = response.args(0);
                    std::cout << "群组创建成功, 群组ID为: " << group_ID << std::endl;
                    // 将群组信息写入本地缓存
                    comm->handle_create_group(group_ID, group_name);
                } else {
                    // 失败时返回的group ID 是空的
                    std::cout << "创建群组失败, 请重试。" << std::endl;
                }
            }
        } else if (input == "13") {
            // 查看群组成员
            std::cout << "请输入要查看成员的群组ID：" << std::endl;
            print_input_sign();
            std::getline(std::cin, group_ID);
            if (group_ID.empty()) {
                std::cout << "输入为空." << std::endl;
            } else if (comm->cache.group_list.find(group_ID) == comm->cache.group_list.end()) {
                std::cout << "群组不存在, 请检查ID是否正确。" << std::endl;
            } else if (comm->cache.group_members.find(group_ID) == comm->cache.group_members.end()
                        || comm->cache.group_members[group_ID].empty()) {
                std::cout << "群组成员列表未正确加载, 请稍后再试。" << std::endl;
            } else {
                std::vector<std::pair<std::string, bool>> members;
                if (comm->cache.group_members.find(group_ID) != comm->cache.group_members.end()) {
                    members.assign(
                        comm->cache.group_members[group_ID].begin(),
                        comm->cache.group_members[group_ID].end()
                    );
                } else {
                    members = comm->sqlite_con->get_cached_group_members_with_admin_status(group_ID);
                    for (auto& [member, is_admin] : members) {
                        comm->cache.group_members[group_ID][member] = is_admin;
                    }
                }
                std::cout << "群组 " << group_ID << " 的成员列表：" << std::endl;
                std::cout << "共计 " << members.size() << " 名成员。" << std::endl;
                auto owner = comm->cache.group_list[group_ID].owner_ID;
                std::cout <<
                    (owner == comm->cache.user_ID ? style(owner, {ansi::BOLD, ansi::FG_YELLOW}) : owner)
                    << " (群主)" << std::endl;
                auto new_end = std::remove_if(members.begin(), members.end(),
                    [&](const auto& member) {
                        return member.first == owner;
                    });
                members.erase(new_end, members.end());
                sort(members.begin(), members.end(),
                    [](const auto& a, const auto& b) {
                        // 管理员优先，然后按字母顺序排序
                        if (a.second != b.second) {
                            return a.second > b.second; // true if a is admin and b is not
                        }
                        return a.first < b.first; // 按字母顺序排序
                    });
                for (const auto& [member, is_admin] : members) {
                    std::cout <<
                        (member == comm->cache.user_ID ? style(member, {ansi::BOLD, ansi::FG_YELLOW}) : member)
                        << (is_admin ? " (管理员)" : "") << std::endl;
                }
            }
        } else if (input == "14") {
            // 查看群组管理员
            std::cout << "请输入要查看管理员的群组ID：" << std::endl;
            print_input_sign();
            std::getline(std::cin, group_ID);
            if (group_ID.empty()) {
                std::cout << "输入为空." << std::endl;
            } else if (comm->cache.group_list.find(group_ID) == comm->cache.group_list.end()) {
                std::cout << "群组不存在, 请检查ID是否正确。" << std::endl;
            } else if (comm->cache.group_members.find(group_ID) == comm->cache.group_members.end()
                        || comm->cache.group_members[group_ID].empty()) {
                std::cout << "群组成员列表未正确加载, 请稍后再试。" << std::endl;
            } else {
                std::vector<std::pair<std::string, bool>> members;
                if (comm->cache.group_members.find(group_ID) != comm->cache.group_members.end()) {
                    members.assign(
                        comm->cache.group_members[group_ID].begin(),
                        comm->cache.group_members[group_ID].end()
                    );
                } else {
                    members = comm->sqlite_con->get_cached_group_members_with_admin_status(group_ID);
                    for (auto& [member, is_admin] : members) {
                        comm->cache.group_members[group_ID][member] = is_admin;
                    }
                }
                std::cout << "群组 " << group_ID << " 的管理员列表：" << std::endl;
                auto owner = comm->cache.group_list[group_ID].owner_ID;
                auto new_end = std::remove_if(members.begin(), members.end(),
                    [&](const auto& member) {
                        return member.first == owner;
                    });
                members.erase(new_end, members.end());
                std::cout << "群主: "
                    << (owner == comm->cache.user_ID ? style(owner, {ansi::BOLD, ansi::FG_YELLOW}) : owner)
                    << std::endl;
                for (const auto& [member, is_admin] : members) {
                    if (is_admin)
                        std::cout <<
                            (member == comm->cache.user_ID ? style(member, {ansi::BOLD, ansi::FG_YELLOW}) : member)
                            << (is_admin ? " (管理员)" : " (成员)") << std::endl;
                }
            }
        } else if (input == "15") {
            // 邀请好友加入群聊
            std::cout << "请输入要邀请的好友ID：" << std::endl;
            print_input_sign();
            std::getline(std::cin, user_ID);
            if (user_ID.empty()) {
                std::cout << "输入为空." << std::endl;
            } else if (user_ID == comm->cache.user_ID) {
                std::cout << "你不能邀请自己。" << std::endl;
            } else if (comm->cache.friend_list.find(user_ID) == comm->cache.friend_list.end()) {
                std::cout << "好友不存在, 请检查ID是否正确。" << std::endl;
            } else {
                std::cout << "请输入要邀请的群组ID：" << std::endl;
                print_input_sign();
                std::getline(std::cin, group_ID);
                if (group_ID.empty()) {
                    std::cout << "输入为空." << std::endl;
                } else if (comm->cache.group_list.find(group_ID) == comm->cache.group_list.end()) {
                    std::cout << "群组不存在, 请检查ID是否正确。" << std::endl;
                } else {
                    auto& group_name = comm->cache.group_list[group_ID].group_name;
                    comm->handle_send_command(Action::Invite_To_Group_Req,
                        comm->cache.user_ID,
                        {TimeUtils::current_time_string(), group_ID, group_name, user_ID}, false);
                    std::cout << "已发送邀请好友加入群组请求。" << std::endl;
                }
            }
        } else if (input == "16") {
            // 踢人
            std::cout << "请输入群组ID：" << std::endl;
            print_input_sign();
            std::getline(std::cin, group_ID);
            if (group_ID.empty()) {
                std::cout << "群组ID不能为空" << std::endl;
            } else if (comm->cache.group_list.find(group_ID) == comm->cache.group_list.end()) {
                std::cout << "群组不存在, 请检查ID是否正确。" << std::endl;
            } else if (comm->cache.group_members.find(group_ID) == comm->cache.group_members.end()
                        || comm->cache.group_members[group_ID].empty()) {
                std::cout << "群组成员列表未正确加载, 请稍后再试。" << std::endl;
            } else if (!comm->cache.group_list[group_ID].is_user_admin) {
                std::cout << "你不是管理员。" << std::endl;
                continue;
            } else {
                std::cout << "请输入要踢出的用户ID：" << std::endl;
                print_input_sign();
                std::getline(std::cin, user_ID);
                if (user_ID.empty()) {
                    std::cout << "成员ID不能为空" << std::endl;
                } else if (user_ID == comm->cache.user_ID) {
                    std::cout << "你不能移除自己, 请使用/quit退出群组。" << std::endl;
                } else if (comm->cache.group_members[group_ID].find(user_ID) == comm->cache.group_members[group_ID].end()) {
                    std::cout << "这个账号不是群组成员。" << std::endl;
                } else {
                    comm->handle_send_command(Action::Remove_From_Group,
                    comm->cache.user_ID,
                        {TimeUtils::current_time_string(), group_ID, user_ID}, false);
                    std::cout << "已发送移除成员请求。" << std::endl;
                    // 从本地缓存中删除
                    comm->handle_remove_person(group_ID, user_ID);
                }
            }
        } else if (input == "0") {
            switch_to(UIPage::Contacts);
            return;
        } else {
            std::cout << "无效的选项，请重新输入。" << std::endl;
        }
        pause();
        continue;
    }
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
        std::cout << "无效的输入, 请重新选择。" << std::endl;
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
        comm->cache.user_ID.clear();
        comm->cache.user_email.clear();
        comm->cache.user_password_hash.clear();
        comm->cache.friend_list.clear();
        comm->cache.group_list.clear();
        comm->cache.group_members.clear();
        comm->cache.notices.clear();
        comm->cache.requests.clear();
        comm->cache.real_time_notices.clear();
        comm->cache.conversations.clear();
        comm->cache.current_conversation_id.clear();
        comm->cache.messages.clear();
        comm->cache.new_messages.clear();
        comm->cache.sorted_conversation_list.clear();
        std::cout << "正在退出登录..." << std::endl;
        comm->online = false;
        comm->clients[0]->socket->set_nonblocking(0);
        comm->clients[1]->socket->set_nonblocking(0);
        switch_to(UIPage::Start);
    } else {
        std::cout << "无效的输入, 请重新选择。" << std::endl;
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
    } else if (input == "3") {
        switch_to(UIPage::My_Lists);
    } else if (input == "0") {
        switch_to(UIPage::Main);
    } else {
        std::cout << "无效的输入, 请重新选择。" << std::endl;
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
            std::cout << "用户名只能包含字母、数字和下划线, 长度在5-20位之间";
            std::cout << "用户名";
            print_input_sign();
            break;
        }
        case 4: {
            std::cout << "密码长度8-16位, 至少包含一个大写字母、小写字母、\n";
            std::cout << "数字和特殊字符, 且不能包含空格, 在ascii 127范围内" << std::endl;
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
    std::cout << selnum(0) + " 退出登录" << std::endl;
    print_input_sign();
}

void draw_contacts(std::mutex& mtx, CommManager* comm) {
    std::lock_guard<std::mutex> lock(mtx);
    std::cout << "-$- 联系人列表 -$-" << std::endl;
    std::cout << selnum(1) + " 查看通知(" << comm->cache.notices.size() << ")" << std::endl;
    std::cout << selnum(2) + " 管理请求(" << comm->cache.requests.size() << ")" << std::endl;
    std::cout << selnum(3) + " 查看我的列表" << std::endl;
    std::cout << selnum(0) + " 返回主菜单" << std::endl;
    print_input_sign();
}

void draw_my_lists(std::mutex& mtx, CommManager* comm) {
    std::cout << "-$- 我的列表 -$-" << std::endl;
    std::cout << selnum(1) + " 好友列表(" << comm->cache.friend_list.size() << ")" << std::endl;
    std::cout << selnum(2) + " 群组列表(" << comm->cache.group_list.size() << ")" << std::endl;
    std::cout << selnum(3) + " 添加好友" << std::endl;
    std::cout << selnum(4) + " 加入群聊" << std::endl;
    std::cout << selnum(5) + " 删除好友" << std::endl;
    std::cout << selnum(6) + " 屏蔽好友" << std::endl;
    std::cout << selnum(7) + " 取消屏蔽好友" << std::endl;
    std::cout << selnum(8) + " 退出群组" << std::endl;
    std::cout << selnum(9) + " 解散群组" << std::endl;
    std::cout << selnum(10) + " 移除管理员" << std::endl;
    std::cout << selnum(11) + " 添加管理员" << std::endl;
    std::cout << selnum(12) + " 创建群组" << std::endl;
    std::cout << selnum(13) + " 显示群组成员" << std::endl;
    std::cout << selnum(14) + " 显示群组管理员" << std::endl;
    std::cout << selnum(15) + " 邀请好友加入群组" << std::endl;
    std::cout << selnum(16) + " 移除群组成员" << std::endl;
    std::cout << selnum(0) + " 返回联系人菜单" << std::endl;
    print_input_sign();
}

/* ---------- 聊天功能实现 ---------- */

void WinLoop::display_conversation_list(
    const std::vector<std::pair<std::string, ContactCache::ConversationInfo*>>& conv_list
) {
    for (size_t i = 0; i < conv_list.size(); ++i) {
        const auto& info = *conv_list[i].second;
        std::cout << selnum(i + 1) << " ";

        // 显示名称和未读计数
        std::cout << std::left << std::setw(20) << info.name;
        if (info.unread_count > 0) {
            std::cout << " (" << info.unread_count << ")";
        }

        // 显示最后消息预览
        if (!info.last_message.empty()) {
            std::string preview = info.last_message;
            if (preview.length() > 30) {
                preview = preview.substr(0, 27) + "...";
            }
            std::cout << " | " << preview;
        }

        std::cout << std::endl;
    }
}

void WinLoop::display_chat_messages(const std::string& conversation_id) {
    auto messages = comm->get_conversation_messages(conversation_id);

    if (messages.empty()) {
        std::cout << "暂无消息历史" << std::endl;
        return;
    }

    for (const auto& msg : messages) {
        std::cout << format_message(msg) << std::endl;
    }
}

std::string WinLoop::format_message(const ChatMessage& msg) {
    std::string result;

    // 消息发送者和时间
    std::string sender = msg.sender();
    auto timestamp = msg.timestamp();

    // 转换时间戳为可读格式
    std::time_t time_val = static_cast<std::time_t>(timestamp);
    std::tm* tm_info = std::localtime(&time_val);
    char time_str[100];
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    // 构建消息格式 [发送者]<时间>
    result += "[" + sender + "]<" + time_str + ">\n";

    if (msg.pin()) {
        // 文件消息
        result += "~~(" + msg.payload().file_name() + ")["
            + msg.payload().file_hash().substr(0, 6);
        result += ", " + std::to_string(msg.payload().file_size() / 1024.0) + "KB]~~";
    } else {
        // 文本消息
        result += msg.text();
    }

    return result;
}

void WinLoop::print_header() {
    std::string header = "[" + style(comm->cache.user_ID, {ansi::BOLD, ansi::FG_BRIGHT_YELLOW}) + "]";
    if (comm->cache.group_list.find(comm->cache.current_conversation_id) != comm->cache.group_list.end()) {
        const auto& group = comm->cache.group_list[comm->cache.current_conversation_id];
        if (comm->cache.user_ID == group.owner_ID) {
            header += "[群主]";
        } else if (group.is_user_admin) {
            header += "[管理员]";
        }
    }
    std::cout << header;
    std::cout.flush();
}

void WinLoop::render_message(const ChatMessage& msg) {
    std::string header = "[" + style(msg.sender(), {ansi::BOLD, ansi::FG_BRIGHT_BLUE}) + "]";
    if (comm->cache.group_list.find(msg.receiver()) != comm->cache.group_list.end() && msg.is_group()) {
        if (msg.sender() == comm->cache.group_list[msg.receiver()].owner_ID) {
            header += "[群主]";
        } else if (comm->cache.group_members[msg.receiver()].find(msg.sender()) !=
                   comm->cache.group_members[msg.receiver()].end() &&
                   comm->cache.group_members[msg.receiver()][msg.sender()]) {
            header += "[管理员]";
        }
    }

    // 时间戳转格式化时间
    std::time_t t = msg.timestamp();
    char time_buf[64];
    std::strftime(time_buf, sizeof(time_buf), "%Y.%m.%d %H:%M:%S", std::localtime(&t));
    std::string time_part = "<" + std::string(time_buf) + ">";

    std::cout << '\r' << header << style(time_part, {ansi::BOLD, ansi::FG_GREEN}) << std::endl;

    // 文本消息
    if (!msg.text().empty()) {
        std::istringstream iss(msg.text());
        std::string line;
        while (std::getline(iss, line)) {
            std::cout << line << "\n";
        }
    }

    // 文件信息
    if (msg.pin()) {
        const auto& file = msg.payload();
        auto name = file.file_name();
        size_t at_pos = name.find_last_of('@');
        std::string display_name, file_id;

        if (at_pos != std::string::npos) {
            // 找到@符号，分离文件名和ID
            display_name = name.substr(0, at_pos);
            file_id = name.substr(at_pos + 1);
        } else {
            // 没有@符号，无法获取文件ID
            display_name = name;
            file_id = "未知"; // 不再使用hash前8位作为ID
        }

        std::string file_line = "~~(" + display_name + ")[ID:" + file_id + ", ";
        double size_kb = file.file_size() / 1024.0;
        std::ostringstream oss;
        if (size_kb < 1024) {
            oss << std::fixed << std::setprecision(1) << size_kb << "KB";
        } else {
            oss << std::fixed << std::setprecision(1) << size_kb / 1024.0 << "MB";
        }
        file_line += oss.str() + "]~~";
        std::cout << style(file_line, {ansi::FG_BRIGHT_RED}) << "\n";
    }

    std::cout << std::endl;  // 空一行
}
