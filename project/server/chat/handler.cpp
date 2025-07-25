#include "../include/handler.hpp"
#include "../include/email.hpp"
#include "../include/TcpServerConnection.hpp"
#include "../include/TcpServer.hpp"
#include "../include/dispatcher.hpp"
#include "../../global/include/logging.hpp"
#include "../../global/abstract/datatypes.hpp"
#include "../include/connection_manager.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

/* Handler base */
Handler::Handler(Dispatcher* dispatcher) : disp(dispatcher) {}

/* ---------- MessageHandler ---------- */

MessageHandler::MessageHandler(Dispatcher* dispatcher) : Handler(dispatcher) {}

void MessageHandler::handle_recv(const ChatMessage& message, const std::string& ostr) {
    std::string sender = message.sender();
    std::string receiver = message.receiver();
    bool is_group = message.is_group();
    auto recv_status = disp->redis_con->get_user_status(receiver);

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
        case Action::Find_Password: {
            handle_find_password();
            break;
        }
        case Action::Change_Password: {
            handle_change_password();
            break;
        }
        case Action::Change_Username: {
            handle_change_username();
            break;
        }
        case Action::Authentication: {
            handle_authentication(conn, subj, args[0]);
            break;
        }
        case Action::Accept: {
            handle_accept();
            break;
        }
        case Action::Refuse: {
            handle_refuse();
            break;
        }
        case Action::Add_Friend: {
            handle_add_friend();
            break;
        }
        case Action::Remove_Friend: {
            handle_remove_friend();
            break;
        }
        case Action::Search_Person: {
            handle_search_person();
            break;
        }
        case Action::Create_Group: {
            handle_create_group();
            break;
        }
        case Action::Join_Group: {
            handle_join_group();
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
        case Action::Invite_To_Group: {
            handle_invite_to_group();
            break;
        }
        case Action::Remove_From_Group: {
            handle_remove_from_group();
            break;
        }
        case Action::Search_Group: {
            handle_search_group();
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
        case Action::Get_Relation_Net: {
            handle_get_relation_net(subj);
            break;
        }
        case Action::Download_File: {
            handle_download_file();
            break;
        }
        case Action::Remember_Connection: {
            // 注册连接行为
            conn->user_ID = subj; // 这里的subj是user_ID
            int server_index = std::stoi(args[0]);
            disp->conn_manager->add_conn(conn, server_index);
        }
        default: {
            log_error("Unknown action received: Action_ID={}", static_cast<int>(action));
            break;
        }
    }
}

void CommandHandler::handle_send(TcpServerConnection* conn) {
    conn->socket->send_with_size();
    log_debug("CommandHandler::handle_send called");
}

void CommandHandler::handle_sign_in(
    TcpServerConnection* conn,
    const std::string& email,
    const std::string& password_hash) {
    log_debug("handle_sign_in called");
    bool res = disp->mysql_con->check_user_pswd(email, password_hash);
    if (res) {
        // 登录成功
        // 通知客户端
        CommandRequest cmd;
        auto env_out = create_command_string(
            Action::Accept, email, {});
        conn->socket->set_write_buf(env_out);
        conn->write_event->add_to_reactor();
        // 更新mysql的last_active
        disp->mysql_con->update_user_last_active(email);
        // 更新redis的在线状态
        std::string user_ID = disp->mysql_con->get_user_id_from_email(email);
        // 一般不会获取失败，除非客户端寄了
        disp->redis_con->set_user_status(user_ID, "active");
    } else {
        // 登录失败
        std::string err_msg = "用户名或密码错误";
        auto env_out = create_command_string(
            Action::Refuse, email, {err_msg});
        conn->socket->set_write_buf(env_out);
        conn->write_event->add_to_reactor();
    }
}

void CommandHandler::handle_sign_out(const std::string& user_ID) {
    disp->redis_con->set_user_status(user_ID, false);
    disp->conn_manager->remove_user(user_ID);
    disp->mysql_con->update_user_status(user_ID, false);
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
            // 通知他注册成功了
            // ...
            CommandRequest cmd;
            auto env_out = create_command_string(
                Action::Accept, email, {"注册成功"});
            // 注册事件
            conn->socket->set_write_buf(env_out);
            conn->write_event->add_to_reactor();
            return;
        }
        err_msg = "注册失败，请稍后再试";
    }
    // 通知他不能注册(Refuse)
    if (err_msg.empty()) err_msg = "用户名已存在";
    std::string env_out = create_command_string(
        Action::Refuse, email, {err_msg});
    // 注册事件
    conn->socket->set_write_buf(env_out);
    conn->write_event->add_to_reactor();
}

void CommandHandler::handle_send_veri_code(
    TcpServerConnection* conn,
    std::string subj) {
    bool email_exists = disp->mysql_con->do_email_exist(subj);
    if (email_exists) {
        auto env_out = create_command_string(
            Action::Refuse, subj, {"邮箱已存在"});
        conn->socket->set_write_buf(env_out);
        conn->write_event->add_to_reactor();
        return;
    }
    const std::string qq_email = "decglu@qq.com";
    const std::string auth_code = "gowkltdykhdmgehh";
    std::string veri_code = std::to_string(rand() % 900000 + 100000); // 生成6位随机验证码
    try {
        QQMailSender sender;
        sender.init(qq_email, auth_code);
        sender.set_content(
            "聊天室账户验证",
            {subj},
            "尊敬的用户：\n\n"
            "感谢您使用聊天室(チャットルーム)服务！\n"
            "您的验证码是：" + veri_code + "\n\n"
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
            // 发送成功，通知客户端
            auto env_out = create_command_string(
                Action::Accept, subj, {});
            conn->socket->set_write_buf(env_out);
            conn->write_event->add_to_reactor();
            return;
        } else {
            log_error("!!! 邮件发送失败: " + sender.get_error() + " !!!");
        }
        auto env_out = create_command_string(
            Action::Refuse, subj, {"暂时无法使用验证服务"});
        conn->socket->set_write_buf(env_out);
        conn->write_event->add_to_reactor();
    } catch (const std::exception& e) {
        std::cerr << "初始化错误: " << e.what() << std::endl;
        return;
    }
}

void CommandHandler::handle_find_password() {}

void CommandHandler::handle_change_password() {}

void CommandHandler::handle_change_username() {}

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
            Action::Accept, email, {"身份验证成功"});
        conn->socket->set_write_buf(env_out);
        conn->write_event->add_to_reactor();
    } else {
        auto env_out = create_command_string(
            Action::Refuse, email, {"验证码错误"});
        conn->socket->set_write_buf(env_out);
        conn->write_event->add_to_reactor();
    }
}

void CommandHandler::handle_refuse() {
    log_debug("handle_refuse called");
}

void CommandHandler::handle_accept() {
    log_debug("handle_accept called");
}

void CommandHandler::handle_add_friend() {}

void CommandHandler::handle_remove_friend() {}

void CommandHandler::handle_search_person() {}

void CommandHandler::handle_create_group() {}

void CommandHandler::handle_join_group() {}

void CommandHandler::handle_leave_group() {}

void CommandHandler::handle_disband_group() {}

void CommandHandler::handle_invite_to_group() {}

void CommandHandler::handle_remove_from_group() {}

void CommandHandler::handle_search_group() {}

void CommandHandler::handle_add_admin() {}

void CommandHandler::handle_remove_admin() {}

void CommandHandler::handle_get_relation_net(const std::string& user_ID) {
    log_debug("handle_get_relation_net called for user: {}", user_ID);
    // 构建完整关系网数据的JSON
    json relation_data;
    // 获取好友列表（包含屏蔽状态）
    auto friends_with_status = disp->mysql_con->get_friends_with_block_status(user_ID);
    json friends_array = json::array();
    for (const auto& [friend_id, is_blocked] : friends_with_status) {
        json friend_info;
        friend_info["id"] = friend_id;
        friend_info["blocked"] = is_blocked;
        friends_array.push_back(friend_info);
    }
    relation_data["friends"] = friends_array;
    // 获取群组列表
    auto group_list = disp->mysql_con->get_user_groups(user_ID);
    json groups_array = json::array();
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
        groups_array.push_back(group_info);
    }
    relation_data["groups"] = groups_array;
    // 创建SyncItem进行全量关系网同步
    auto sync_str = create_sync_string(
        SyncItem::RELATION_NET_FULL,
        user_ID,
        relation_data.dump()
    );
    auto data_conn = disp->conn_manager->get_connection(user_ID, 2);
    if (data_conn) {
        data_conn->socket->set_write_buf(sync_str);
        data_conn->write_event->add_to_reactor();
        data_conn->set_send_type(DataType::SyncItem);
        log_info("Full relation network sent to user: {}", user_ID);
    } else {
        log_error("Data connection not found for user: {}", user_ID);
    }
    log_debug("Full relation network data prepared for user: {}", user_ID);
}

void CommandHandler::handle_update_relation_net(const std::string& user_ID) {}

void CommandHandler::handle_download_file() {}

/* ---------- FileHandler ---------- */

FileHandler::FileHandler(Dispatcher* dispatcher) : Handler(dispatcher) {}

void FileHandler::handle_recv(const FileChunk& file_chunk, const std::string& ostr) {
    // 处理文件分片
}

void FileHandler::handle_send(const FileChunk& file_chunk, const std::string& ostr) {
    // 处理文件分片发送
}

/* ---------- SyncHandler ---------- */

SyncHandler::SyncHandler(Dispatcher* dispatcher) : Handler(dispatcher) {}

void SyncHandler::handle_send(const SyncItem& sync_item, const std::string& ostr) {
    // 处理同步数据
}

/* ---------- OfflineMessageHandler ---------- */

OfflineMessageHandler::OfflineMessageHandler(Dispatcher* dispatcher) : Handler(dispatcher) {}

void OfflineMessageHandler::handle_recv(const OfflineMessages& offline_messages, const std::string& ostr) {
    // 处理离线消息
}
