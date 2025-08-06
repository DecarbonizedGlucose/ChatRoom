#include "../include/handler.hpp"
#include "../include/email.hpp"
#include "../include/TcpServerConnection.hpp"
#include "../include/TcpServer.hpp"
#include "../include/dispatcher.hpp"
#include "../../global/include/logging.hpp"
#include "../include/connection_manager.hpp"
#include <iostream>
#include <regex>
#include "../include/sfile_manager.hpp"

void try_send(ConnectionManager* conn_manager,
    TcpServerConnection* conn,
    const std::string& proto,
    DataType type
) {
    // 首先设置要发送的数据
    conn->socket->set_write_buf(proto);
    conn->set_send_type(type);

    // 先尝试立即发送数据
    ssize_t sent = conn->socket->send_with_size();
    if (sent == 0) {
        // 可能是缓冲区空或者socket不可写, 注册写事件等待
        //log_debug("Socket not ready for writing or no data, registering write event for fd: {}", conn->socket->get_fd());
        conn->write_event->add_to_reactor();
    } else if (sent > 0) {
        // 数据发送成功
        if (!conn->user_ID.empty())
            conn_manager->update_user_activity(conn->user_ID);
        log_debug("Sent immediately to fd={}, bytes={}, user_ID={}", conn->socket->get_fd(), sent, conn->user_ID);
    } else {
        // 发送出错 (sent < 0)
        log_error("Failed to send (fd:{}): {}", conn->socket->get_fd(), strerror(errno));
    }
}

/* Handler base */
Handler::Handler(Dispatcher* dispatcher) : disp(dispatcher) {}

/* ---------- MessageHandler ---------- */

MessageHandler::MessageHandler(Dispatcher* dispatcher) : Handler(dispatcher) {}

void MessageHandler::handle_recv(const ChatMessage& message, const std::string& ostr) {
    std::string sender = message.sender();
    std::string receiver = message.receiver();
    bool is_group = message.is_group();

    if (!is_group) {
        // 判断是不是他好友
        if (disp->redis_con->is_friend(sender, receiver)) {
            // 判断有没有被对方屏蔽
            if (!disp->redis_con->is_blocked_by_friend(sender, receiver)) {
                // 判断是否在线
                if (disp->redis_con->get_user_status(receiver).first) {
                    auto conn = disp->conn_manager->get_connection(receiver);
                    if (conn) {
                        // 在线, 直接发送
                        try_send(disp->conn_manager, conn, ostr);
                    }
                }
                // 不在线
            } else {
                // 被屏蔽了
                return;
            }
        } else {
            return;
        }
    } else { // 群组消息
        // 判断他在不在群里
        if (disp->redis_con->is_group_member(receiver, sender)) {
            // 在群里, 对所有人发送
            auto members = disp->redis_con->
                get_group_members(receiver);
            for (const auto& member_id : members) {
                if (member_id == sender) continue; // 不发给自己
                auto conn = disp->conn_manager->get_connection(member_id);
                if (conn) {
                    try_send(disp->conn_manager, conn, ostr);
                }
                // 离线
            }
        }
        else {
            // 不在群里
            // auto conn = disp->conn_manager->get_connection(sender, 1);
            // auto env_str = create_command_string(
            //     Action::Warn,
            //     "ChatRoom Team",
            //     { "You are not a member of group " + receiver + ", message not sent." }
            // );
            // try_send(disp->conn_manager, conn, env_str, DataType::Command);
        }
    }
    // 存起来
    disp->mysql_con->add_chat_message(
        sender, receiver, message.is_group(), message.timestamp(),
        message.text(), message.pin(), message.payload().file_name(),
        message.payload().file_size(), message.payload().file_hash()
    );
}

void MessageHandler::handle_send(TcpServerConnection* conn) {
    conn->socket->send_with_size();
}

/* ---------- CommandHandler ---------- */

CommandHandler::CommandHandler(Dispatcher* dispatcher) : Handler(dispatcher) {}

void CommandHandler::handle_recv(
    TcpServerConnection* conn,
    const CommandRequest& command,
    const std::string& ostr) {
    Action action = (Action)command.action();
    std::string subj = command.sender();
    auto args = command.args();

    if (action == Action::HEARTBEAT) {
        // 专门的心跳包
        log_info("Heartbeat received from user: {}", subj);
        return;
    }

    switch (action) {
        case Action::Sign_In: {
            handle_sign_in(conn, subj, args[0]);
            break;
        }
        case Action::Sign_Out: {
            handle_sign_out(subj);
            break;
        }
        case Action::Register: {
            handle_register(conn, subj, args[0], args[1]);
            break;
        }
        case Action::Get_Veri_Code: {
            handle_send_veri_code(conn, subj);
            break;
        }
        case Action::Authentication: {
            handle_authentication(conn, subj, args[0]);
            break;
        }
        case Action::Accept_FReq: {
            handle_accept_friend_request(
                subj, args[1], ostr);
            break;
        }
        case Action::Refuse_FReq: {
            handle_refuse_friend_request(
                subj, args[1], ostr);
            break;
        }
        case Action::Refuse_GReq: {
            handle_refuse_group_request(
                args[1], ostr);
            break;
        }
        case Action::Accept_GReq: {
            handle_accept_group_request(
                args[1], ostr);
            break;
        }
        case Action::Add_Friend_Req: {
            handle_add_friend_req(
                args[1], ostr);
            break;
        }
        case Action::Remove_Friend: {
            handle_remove_friend(subj, args[1], command, ostr);
            break;
        }
        case Action::Search_Person: {
            handle_search_person(conn, args[0]);
            break;
        }
        case Action::Create_Group: {
            handle_create_group(
                args[0], subj, args[0]);
            break;
        }
        case Action::Block_Friend: {
            handle_block_friend(subj, args[0]);
            break;
        }
        case Action::Unblock_Friend: {
            handle_unblock_friend(subj, args[0]);
            break;
        }
        case Action::Join_Group_Req: {
            handle_join_group_req();
            break;
        }
        case Action::Leave_Group: {
            handle_leave_group();
            break;
        }
        case Action::Disband_Group: {
            handle_disband_group();
            break;
        }
        case Action::Invite_To_Group_Req: {
            handle_invite_to_group_req();
            break;
        }
        case Action::Remove_From_Group: {
            handle_remove_from_group();
            break;
        }
        case Action::Search_Group: {
            handle_search_group(conn, args[0]);
            break;
        }
        case Action::Add_Admin: {
            handle_add_admin();
            break;
        }
        case Action::Remove_Admin: {
            handle_remove_admin();
            break;
        }
        case Action::Upload_File: {
            handle_upload_file(conn, args[0], std::stoul(args[1]));
            break;
        }
        case Action::Download_File: {
            handle_download_file(subj, args[0]);
            break;
        }
        case Action::Remember_Connection: {
            // 注册连接行为
            conn->user_ID = subj; // 这里的subj是user_ID
            int server_index = std::stoi(args[0]);
            disp->conn_manager->add_conn(conn, server_index);
        }
        case Action::Online_Init: {
            handle_online_init(subj);
            break;
        }
        default: {
            log_error("Unknown action received: Action_ID={}", static_cast<int>(action));
            break;
        }
    }
}

void CommandHandler::handle_send(TcpServerConnection* conn) {
    auto sent = conn->socket->send_with_size();
    conn->socket->send_with_size();
    log_debug("CommandHandler::handle_send called, sent to fd={}, size={}, user_ID={}", conn->socket->get_fd(), sent, conn->user_ID);
}

void CommandHandler::handle_sign_in(
    TcpServerConnection* conn,
    const std::string& subj,
    const std::string& password_hash) {
    log_debug("handle_sign_in called");
    // 判断subj是邮箱还是用户名
    const std::regex pattern(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
    bool is_email = std::regex_match(subj, pattern);
    std::string ret;
    bool res;
    if (is_email) {
        res = disp->mysql_con->check_user_pswd(subj, password_hash);
        ret = disp->mysql_con->get_user_id_from_email(subj); // user_ID
    } else {
        ret = disp->mysql_con->get_user_email_from_id(subj); // email
        res = disp->mysql_con->check_user_pswd(ret, password_hash);
    }
    auto user_ID = is_email ? ret : subj; // user_ID
    auto user_email = is_email ? subj : ret; // email
    std::string err_msg;
    if (disp->redis_con->get_user_status(user_ID).first) {
        res = false;
        err_msg = "用户已在线";
    }
    if (res) {
        // 登录成功
        auto env_out = create_command_string(
            Action::Accept_Login, "", {ret});
            conn->user_ID = user_ID; // 设置连接的user_ID
        try_send(disp->conn_manager, conn, env_out);
    } else {
        // 登录失败
        if (err_msg.empty()) err_msg = "用户名/邮箱与密码不匹配";
        auto env_out = create_command_string(
            Action::Refuse_Login, "", {err_msg});
        try_send(disp->conn_manager, conn, env_out);
        return;
    }
    // 通知所有在线好友
    auto friends = disp->mysql_con->get_friends_list(user_ID);
    for (const auto& friend_ID : friends) {
        if (!disp->redis_con->get_user_status(friend_ID).first)
            continue; // 不在线
        auto env_out = create_command_string(
            Action::Friend_Online, "", {user_ID});
        auto friend_conn = disp->conn_manager->get_connection(friend_ID, 1);
        if (friend_conn) { // 好友在线
            try_send(disp->conn_manager, friend_conn, env_out);
        }
    }
}

void CommandHandler::handle_sign_out(const std::string& user_ID) {
    log_debug("handle_sign_out called for user_ID: {}", user_ID);

    // 先通知所有在线好友, 再删除用户连接
    // 这样避免在通知过程中访问已删除的连接
    try {
        auto friends = disp->mysql_con->get_friends_list(user_ID);
        for (const auto& friend_ID : friends) {
            try {
                if (!disp->redis_con->get_user_status(friend_ID).first)
                    continue; // 好友不在线, 跳过
                auto env_out = create_command_string(Action::Friend_Offline, "", {user_ID});
                auto friend_conn = disp->conn_manager->get_connection(friend_ID, 1);
                if (friend_conn) { // 好友在线, 发送离线通知
                    try_send(disp->conn_manager, friend_conn, env_out);
                    log_debug("Notified friend {} about user {} going offline", friend_ID, user_ID);
                } else {
                    log_debug("Friend {} not connected, skipping offline notification", friend_ID);
                }
            } catch (const std::exception& e) {
                log_error("Error notifying friend {} about user {} logout: {}", friend_ID, user_ID, e.what());
            }
        }
    } catch (const std::exception& e) {
        log_error("Error getting friends list for user {} during logout: {}", user_ID, e.what());
    }

    // 最后删除用户连接（这会删除所有相关的连接对象）
    disp->conn_manager->remove_user(user_ID);
    log_info("User {} signed out successfully", user_ID);
}

void CommandHandler::handle_register(
    TcpServerConnection* conn,
    const std::string& email,
    std::string& user_ID,
    std::string& user_password) {
    log_debug("handle_register called");
    std::string err_msg;
    bool user_ID_exists = disp->mysql_con->do_user_id_exist(user_ID);
    if (!user_ID_exists) {
        if (disp->mysql_con->insert_user(user_ID, email, user_password)) {
            // 注册成功
            CommandRequest cmd;
            auto env_out = create_command_string(
                Action::Accept_Regi, "", {});
            try_send(disp->conn_manager, conn, env_out);
            return;
        }
        err_msg = "注册失败, 请稍后再试";
    }
    // 通知他不能注册(Refuse)
    if (err_msg.empty()) err_msg = "用户名已存在";
    std::string env_out = create_command_string(
        Action::Refuse_Regi, "", {err_msg});
    try_send(disp->conn_manager, conn, env_out);
}

void CommandHandler::handle_send_veri_code(
    TcpServerConnection* conn,
    std::string subj) {
    bool email_exists = disp->mysql_con->do_email_exist(subj);
    if (email_exists) {
        auto env_out = create_command_string(
            Action::Refuse_Post_Code, "", {"邮箱已存在"});
        try_send(disp->conn_manager, conn, env_out);
        return;
    }
    const std::string qq_email = "decglu@qq.com";
    const std::string auth_code = "gowkltdykhdmgehh";
    std::string veri_code = std::to_string(rand() % 900000 + 100000); // 生成6位随机验证码
    try {
        QQMailSender sender;
        sender.init(qq_email, auth_code);
        sender.set_content(
            "[C++聊天室项目测试]"
            "聊天室账户验证",
            {subj},
            "尊敬的用户：\n\n"
            "感谢您使用聊天室(チャットルーム)服务！\n"
            "您的验证码是：" + veri_code + "\n\n"
            "如非本人操作, 请忽略本邮件。\n"
            "请勿将此验证码分享给他人。\n"
            "此验证码5分钟后失效。\n\n"
            "感谢您的支持！\n"
            "聊天室团队"
        );
        if (sender.send()) {
            log_info("=== 邮件发送成功！===");
            // 将验证码存入Redis
            disp->redis_con->set_veri_code(subj, veri_code);
            log_info("验证码已存入Redis");
            // 发送成功, 通知客户端
            auto env_out = create_command_string(
                Action::Accept_Post_Code, "", {});
            try_send(disp->conn_manager, conn, env_out);
            return;
        } else {
            log_error("!!! 邮件发送失败: " + sender.get_error() + " !!!");
        }
        auto env_out = create_command_string(
            Action::Refuse_Post_Code, "", {"暂时无法使用验证服务"});
        try_send(disp->conn_manager, conn, env_out);
    } catch (const std::exception& e) {
        std::cerr << "初始化错误: " << e.what() << std::endl;
        return;
    }
}

void CommandHandler::handle_authentication(
    TcpServerConnection* conn,
    const std::string& email,
    const std::string& veri_code) {
    log_debug("handle_authentication called");
    std::string msg;
    std::string real_veri_code = disp->redis_con->get_veri_code(email);
    if (real_veri_code == veri_code) {
        // 撤下验证码
        disp->redis_con->del_veri_code(email);
        auto env_out = create_command_string(
            Action::Success_Auth, "", {});
        try_send(disp->conn_manager, conn, env_out);
    } else {
        auto env_out = create_command_string(
            Action::Failed_Auth, "", {"验证码错误"});
        try_send(disp->conn_manager, conn, env_out);
    }
}

void CommandHandler::handle_refuse_friend_request(
        const std::string& sender,
        const std::string& ori_user_ID,
        const std::string& ostr) {
    log_debug("handle_refuse_friend_request called");
    auto user_online = disp->redis_con->get_user_status(ori_user_ID);
    if (user_online.first) {
        auto data_conn = disp->conn_manager->get_connection(ori_user_ID, 1);
        if (data_conn) {
            try_send(
                disp->conn_manager,
                data_conn,
                ostr
            );
            log_debug("Refuse friend request sent to online user: {}", ori_user_ID);
        }
    }
    // 不在线, 存到mysql
    // ...
}

void CommandHandler::handle_accept_friend_request(
        const std::string& sender,
        const std::string& ori_user_ID,
        const std::string& ostr) {
    log_debug("handle_accept_friend_request called");
    auto user_online = disp->redis_con->get_user_status(ori_user_ID);
    if (user_online.first) {
        auto data_conn = disp->conn_manager->get_connection(ori_user_ID, 1);
        if (data_conn) {
            try_send(
                disp->conn_manager,
                data_conn,
                ostr
            );
            log_debug("Accept friend request sent to online user: {}", ori_user_ID);
        }
    }
    // 服务器mysql更新
    disp->mysql_con->add_friend(ori_user_ID, sender);
    disp->mysql_con->add_friend(sender, ori_user_ID);
    // 服务器redis更新
    if (disp->redis_con->get_user_status(ori_user_ID).first) {
        disp->redis_con->add_friend(ori_user_ID, sender, false);
    }
    // 这个用户一定在线
    disp->redis_con->add_friend(sender, ori_user_ID, false);
}

void CommandHandler::handle_refuse_group_request(
        const std::string& ori_user_ID,
        const std::string& ostr) {
    log_debug("handle_refuse_group_request called");
}

void CommandHandler::handle_accept_group_request(
        const std::string& ori_user_ID,
        const std::string& ostr) {
    log_debug("handle_accept_group_request called");
}

void CommandHandler::handle_add_friend_req(
    const std::string& friend_ID,
    const std::string& ostr) {
    log_debug("handle_add_friend_req called");
    auto user_online = disp->redis_con->get_user_status(friend_ID);
    if (user_online.first) {
        auto data_conn = disp->conn_manager->get_connection(friend_ID, 1);
        if (data_conn) {
            try_send(
                disp->conn_manager,
                data_conn,
                ostr
            );
            //log_debug("Friend request sent to online user: {}", friend_ID);
        }
    }
    // 不在线, 存到mysql
    // ...
}

void CommandHandler::handle_remove_friend(
    const std::string& user_ID,
    const std::string& friend_ID,
    const CommandRequest& cmd,
    const std::string& ostr
) {
    log_debug("handle_remove_friend called for user_ID: {}, friend_ID: {}", user_ID, friend_ID);
    // 先从Redis中删除好友关系
    disp->redis_con->remove_friend(user_ID, friend_ID);
    if (disp->redis_con->get_user_status(friend_ID).first) {
        // 好友在线
        disp->redis_con->remove_friend(friend_ID, user_ID);
        try_send(
            disp->conn_manager,
            disp->conn_manager->get_connection(friend_ID, 1),
            ostr
        );
    } else {
        // 命令存到mysql
        // ...
    }
    // 数据存到mysql
    disp->mysql_con->delete_friend(user_ID, friend_ID);
    disp->mysql_con->delete_friend(friend_ID, user_ID);
    log_info("Removed friend relationship between {} and {}", user_ID, friend_ID);
}

void CommandHandler::handle_search_person(
    TcpServerConnection* conn, const std::string& searched_ID) {
    log_debug("handle_search_person called for user");
    bool user_exists = disp->mysql_con->do_user_id_exist(searched_ID);
    std::string env_str;
    if (user_exists) {
        env_str = create_command_string(
            Action::Notify_Exist, conn->user_ID, {searched_ID});
        log_debug("User {} exists, sending Notify_Exist", searched_ID);
    } else {
        env_str = create_command_string(
            Action::Notify_Not_Exist, conn->user_ID, {searched_ID});
        log_debug("User {} does not exist, sending Notify_Not_Exist", searched_ID);
    }
    try_send(disp->conn_manager, conn, env_str);
}

void CommandHandler::handle_block_friend(
    const std::string& user_ID,
    const std::string& friend_ID) {
    if (disp->redis_con->get_user_status(friend_ID).first) {
        // 好友在线, 存redis
        disp->redis_con->set_blocked_by_friend(user_ID, friend_ID, true);
    }
    // 数据存到mysql
    disp->mysql_con->block_friend(user_ID, friend_ID);
    log_debug("Blocked friend relationship between {} and {}", user_ID, friend_ID);
}

void CommandHandler::handle_unblock_friend(
    const std::string& user_ID,
    const std::string& friend_ID) {
    if (disp->redis_con->get_user_status(friend_ID).first) {
        // 好友在线, 存redis
        disp->redis_con->set_blocked_by_friend(user_ID, friend_ID, false);
    }
    // 数据存到mysql
    disp->mysql_con->unblock_friend(user_ID, friend_ID);
    log_debug("Unblocked friend relationship between {} and {}", user_ID, friend_ID);
}

void CommandHandler::handle_create_group(
    const std::string& group_name,
    const std::string& user_ID,
    const std::string& time) {
    log_debug("handle_create_group called");
    // 创建群组

}

void CommandHandler::handle_join_group_req() {}

void CommandHandler::handle_leave_group() {}

void CommandHandler::handle_disband_group() {}

void CommandHandler::handle_invite_to_group_req() {}

void CommandHandler::handle_remove_from_group() {}

void CommandHandler::handle_search_group(
    TcpServerConnection* conn, const std::string& group_ID) {
    log_debug("handle_search_group called for group: {}", group_ID);
    bool group_exists = disp->mysql_con->search_group(group_ID);
    std::string env_str;
    if (group_exists) {
        env_str = create_command_string(
            Action::Notify_Exist, conn->user_ID, {group_ID});
        log_debug("Group {} exists, sending Notify_Exist", group_ID);
    } else {
        env_str = create_command_string(
            Action::Notify_Not_Exist, conn->user_ID, {group_ID});
        log_debug("Group {} does not exist, sending Notify_Not_Exist", group_ID);
    }
    try_send(disp->conn_manager, conn, env_str);
}

void CommandHandler::handle_add_admin() {}

void CommandHandler::handle_remove_admin() {}

void CommandHandler::handle_post_relation_net(const std::string& user_ID, const json& relation_data) {
    log_debug("handle_post_relation_net called for user: {}", user_ID);
    // 创建SyncItem进行全量关系网同步
    auto sync_str = create_sync_string(
        SyncItem::RELATION_NET_FULL,
        relation_data.dump()
    );
    auto data_conn = disp->conn_manager->get_connection(user_ID, 2);
    if (data_conn) {
        try_send(disp->conn_manager, data_conn, sync_str, DataType::SyncItem);
    } else {
        log_error("Data connection not found for user: {}", user_ID);
    }
    //log_debug("Full relation network data prepared for user: {}", user_ID);
}

void CommandHandler::handle_upload_file(
    TcpServerConnection* conn, const std::string& file_hash, size_t file_size) {
    log_debug("handle_upload_file called for file hash: {}", file_hash);

    // 直接尝试生成file_id
    std::string new_file_id = disp->mysql_con->generate_file_id_only(file_hash);

    if (new_file_id.empty()) {
        // 文件已存在，返回Deny_File命令
        log_info("File with hash {} already exists, denying upload", file_hash);

        auto env_str = create_command_string(
            Action::Deny_File,
            "ChatRoom Server",
            {file_hash, "1", new_file_id}
        );
        try_send(disp->conn_manager, conn, env_str);
        return;
    }

    // 文件不存在，返回Accept_File命令，包含新生成的文件ID
    log_info("File with hash {} does not exist, generated new file_id: {}", file_hash, new_file_id);
    auto env_str = create_command_string(
        Action::Accept_File,
        "ChatRoom Server",
        {file_hash, new_file_id}
    );
    try_send(disp->conn_manager, conn, env_str);

    // 写进mysql
    disp->mysql_con->add_file_record(
        file_hash, new_file_id, file_size);
    // 添加文件接收任务到文件管理器
    auto file = std::make_shared<ServerFile>(
        file_hash, new_file_id, new_file_id, file_size,
        disp->file_manager->storage);
    file->open_for_write();
    disp->file_manager->add_upload_task(conn->user_ID, file);
}

void CommandHandler::handle_download_file(
    const std::string& user_ID, const std::string& file_ID) {
    log_debug("handle_download_file called for user: {}, file_ID: {}", user_ID, file_ID);

    // 通过file_ID获取文件信息
    std::string file_hash = disp->mysql_con->get_file_hash_by_id(file_ID);
    if (file_hash.empty()) { // 文件不存在
        log_error("File not found for file_ID: {}", file_ID);

        // 发送错误响应
        auto conn = disp->conn_manager->get_connection(user_ID, 1);
        if (conn) {
            auto env_str = create_command_string(
                Action::Deny_File_Req,
                "ChatRoom Server",
                {file_ID}
            );
            try_send(disp->conn_manager, conn, env_str);
        }
        return;
    }

    // 获取文件的详细信息
    auto file_info = disp->mysql_con->get_file_info(file_ID);
    if (!file_info.has_value()) {
        log_error("Failed to get file info for file_ID: {}", file_ID);

        auto conn = disp->conn_manager->get_connection(user_ID, 1);
        if (conn) {
            auto env_str = create_command_string(
                Action::Deny_File_Req,
                "ChatRoom Server",
                {file_ID}
            );
            try_send(disp->conn_manager, conn, env_str);
        }
        return;
    }

    std::string file_name = file_info->first;  // 文件名
    size_t file_size = file_info->second;      // 文件大小

    log_info("Found file for download: file_ID={}, file_hash={}, name={}, size={}",
             file_ID, file_hash, file_name, file_size);

    // 发送确认消息
    auto conn = disp->conn_manager->get_connection(user_ID, 1);
    if (conn) {
        auto env_str = create_command_string(
            Action::Accept_File_Req,
            "ChatRoom Server",
            {file_ID, file_hash, std::to_string(file_size)}
        );
        try_send(disp->conn_manager, conn, env_str);
    }

    // 将下载任务添加到文件管理器
    disp->file_manager->add_download_task(user_ID, file_ID, file_hash, file_name, file_size);

    log_info("Added download task to SFileManager for user: {}, file: {}", user_ID, file_name);
}

void CommandHandler::handle_remember_connection(
    TcpServerConnection* conn, const std::string& user_ID, int server_index) {
    log_debug("handle_remember_connection {} called for user: {}", server_index, user_ID);
    // 将连接添加到连接管理器
    // 每次登录, 这个会调用三次
    conn->user_ID = user_ID; // 更新连接的user_ID
    disp->conn_manager->add_conn(conn, server_index);
    log_info("Remembered {}'s conn[{}] fd: {}", user_ID, server_index, conn->socket->get_fd());
}

void CommandHandler::handle_online_init(const std::string& user_ID) {
    log_debug("handle_online_init called for user: {}", user_ID);
    json relation_data; // 用于存储关系网数据
    json blocked_info;
    get_relation_net(user_ID, relation_data);
    get_blocked_info(user_ID, relation_data["friends"], blocked_info);
    disp->redis_con->load_user_relations(user_ID, relation_data, blocked_info);
    // 发送最新关系网
    handle_post_relation_net(user_ID, relation_data);
    // 发送所有在线好友的状态
    handle_post_friends_status(user_ID, relation_data["friends"]);
    // 发送用户离线消息（消息记录）
    handle_post_offline_messages(user_ID, relation_data);
    // 发送未接收的通知和未处理的好友请求/群聊邀请等
    handle_post_unordered_noti_and_req(user_ID, relation_data);
    log_info("Online initialization completed for user: {}", user_ID);
}

void CommandHandler::handle_post_friends_status(
    const std::string& user_ID,
    const json& friends) {
    json friend_list;
    for (const auto& friend_info : friends) {
        std::string friend_id = friend_info["id"];
        bool online = disp->redis_con->get_user_status(friend_id).first;
        friend_list.push_back({friend_id, online});
    };
    auto sync_str = create_sync_string(
        SyncItem::ALL_FRIEND_STATUS,
        friend_list.dump()
    );
    auto data_conn = disp->conn_manager->get_connection(user_ID, 2);
    if (data_conn) {
        try_send(
            disp->conn_manager,
            data_conn,
            sync_str,
            DataType::SyncItem
        );
    } else {
        log_error("Data connection not found for user: {}", user_ID);
    }
}

void CommandHandler::handle_post_offline_messages(
    const std::string& user_ID,
    const json& relation_data) {
    log_debug("handle_post_offline_messages called for user: {}", user_ID);

    try {
        std::vector<ChatMessage> chat_messages;

        // 获取用户的last_active时间
        std::time_t last_active = disp->mysql_con->get_user_last_active(user_ID);
        if (last_active == 0) {
            log_debug("No last_active time found for user {}, sending empty offline messages", user_ID);
        } else {
            // 从MySQL获取离线消息（最多200条）
            auto offline_msg_data = disp->mysql_con->get_offline_messages(user_ID, last_active, 200);

            if (offline_msg_data.empty()) {
                log_debug("No offline messages found for user: {}", user_ID);
            } else {
                // 将MySQL数据转换为ChatMessage对象
                for (const auto& msg_tuple : offline_msg_data) {
                    auto [sender_id, receiver_id, is_group, timestamp, text, pin, file_name, file_size, file_hash] = msg_tuple;

                    ChatMessage chat_msg = create_chat_message(
                        sender_id,
                        receiver_id,
                        is_group,
                        timestamp,
                        text,
                        pin,
                        file_name,
                        file_size,
                        file_hash
                    );
                    chat_messages.push_back(chat_msg);
                }
            }
        }

        // 无论是否有消息, 都要发送OfflineMessages（可能为空）
        std::string env_str = create_offline_messages_string(chat_messages);

        // 发送到data连接通道（通道2）
        auto data_conn = disp->conn_manager->get_connection(user_ID, 2);
        if (data_conn) {
            try_send(
                disp->conn_manager,
                data_conn,
                env_str,
                DataType::OfflineMessages
            );
            log_info("Sent {} offline messages to user: {}", chat_messages.size(), user_ID);
        } else {
            log_error("Data connection not found for user: {}", user_ID);
        }

    } catch (const std::exception& e) {
        log_error("Error handling offline messages for user {}: {}", user_ID, e.what());
    }
}

void CommandHandler::handle_post_unordered_noti_and_req(
    const std::string& user_ID, const json& relation_data) {

}

void CommandHandler::get_friends(const std::string& user_ID, json& friends) {
    auto friends_with_status = disp->mysql_con->get_friends_with_block_status(user_ID);
    friends = json::array();
    for (const auto& [friend_id, is_blocked] : friends_with_status) {
        json friend_info;
        friend_info["id"] = friend_id;
        friend_info["blocked"] = is_blocked;
        friends.push_back(friend_info);
    }
}

void CommandHandler::get_groups(const std::string& user_ID, json& groups) {
    auto group_list = disp->mysql_con->get_user_groups(user_ID);
    groups = json::array();
    for (const auto& group_id : group_list) {
        json group_info;
        group_info["id"] = group_id;
        group_info["name"] = disp->mysql_con->get_group_name(group_id);
        group_info["owner"] = disp->mysql_con->get_group_owner(group_id);
        // 获取群成员（包含管理员状态）
        auto members_with_admin = disp->mysql_con->get_group_members_with_admin_status(group_id);
        json members_array = json::array();
        for (const auto& [member_id, is_admin] : members_with_admin) {
            json member_info;
            member_info["id"] = member_id;
            member_info["is_admin"] = is_admin;
            members_array.push_back(member_info);
        }
        group_info["members"] = members_array;
        groups.push_back(group_info);
    }
}

void CommandHandler::get_relation_net(const std::string& user_ID, json& relation_net) {
    json friends, groups;
    get_friends(user_ID, friends);
    get_groups(user_ID, groups);
    relation_net = json::object();
    relation_net["friends"] = friends;
    relation_net["groups"] = groups;
}

void CommandHandler::get_blocked_info(const std::string& user_ID, const json& friends, json& blocked_info) {
    blocked_info = json::object();

    // 遍历所有好友, 查询当前用户是否被每个好友屏蔽
    for (const auto& friend_info : friends) {
        if (friend_info.contains("id")) {
            std::string friend_id = friend_info["id"];

            // 查询friend_id是否屏蔽了user_ID
            // 这相当于查询friend_id的好友列表中user_ID的屏蔽状态
            bool is_blocked_by_friend = disp->mysql_con->is_blocked_by_friend(user_ID, friend_id);

            blocked_info[friend_id] = is_blocked_by_friend;

            log_debug("User {} blocked by {}: {}", user_ID, friend_id, is_blocked_by_friend);
        }
    }

    log_debug("Generated blocked_info for user {}: {}", user_ID, blocked_info.dump());
}

/* ---------- FileHandler ---------- */

FileHandler::FileHandler(Dispatcher* dispatcher) : Handler(dispatcher) {}

void FileHandler::handle_recv(
    TcpServerConnection* conn,
    const FileChunk& file_chunk) {
    // 获取文件指针
    auto file = disp->file_manager->upload_tasks[conn->user_ID].server_file;
    // 写入数据
    std::vector<char> data(file_chunk.data().begin(), file_chunk.data().end());
    file->write_chunk(data, file_chunk.chunk_index());
    if (file->is_complete()) {
        bool success = file->finalize_upload();
        disp->file_manager->upload_tasks.erase(conn->user_ID);
        if (success) {
            log_info("File upload completed successfully: {}", file->file_name);
        } else {
            log_error("File upload failed during finalization: {}", file->file_name);
        }
    }
}

void FileHandler::handle_send(TcpServerConnection* conn) {
    conn->socket->send_with_size();
    log_debug("FileHandler::handle_send called");
}

/* ---------- SyncHandler ---------- */

SyncHandler::SyncHandler(Dispatcher* dispatcher) : Handler(dispatcher) {}

void SyncHandler::handle_send(TcpServerConnection* conn) {
    conn->socket->send_with_size();
    log_debug("SyncHandler::handle_send called");
}

/* ---------- OfflineMessageHandler ---------- */

OfflineMessageHandler::OfflineMessageHandler(Dispatcher* dispatcher) : Handler(dispatcher) {}

void OfflineMessageHandler::handle_send(TcpServerConnection* conn) {
    conn->socket->send_with_size();
    log_debug("OfflineMessageHandler::handle_send called");
}
