#include "../include/handler.hpp"
#include "../include/email.hpp"
#include "../include/TcpServerConnection.hpp"
#include "../include/TcpServer.hpp"
#include "../include/dispatcher.hpp"
#include "../../global/include/logging.hpp"
#include "../../global/abstract/datatypes.hpp"
#include <iostream>

/* Handler base */
Handler::Handler(Dispatcher* dispatcher) : disp(dispatcher) {}

/* ---------- MessageHandler ---------- */

MessageHandler::MessageHandler(Dispatcher* dispatcher) : Handler(dispatcher) {}

void MessageHandler::handle_recv(const ChatMessage& message, const std::string& ostr) {
    std::string sender = message.sender();
    std::string receiver = message.receiver();
    bool is_group = message.is_group();
    auto recv_status = disp->redis_con->get_user_status(receiver);
    if (!recv_status.first) {
        // 用户不在线,需要上线后拉取
        // 可能要搞个消息队列
    } else {
        auto it = disp->server[0]->user_connections.find(receiver);
        if (it != disp->server[0]->user_connections.end()) {
            auto send_conn = it->second;
            send_conn->socket->set_write_buf(ostr);
            send_conn->write_event->add_to_reactor();
            // 这里没考虑群聊的情况 (for item in group)
            // 需要改改
        } else {
            // 没找到连接,可能是用户离线了,或者是bug
            // 需要处理离线消息
        }
    }
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
            break;
        }
        case Action::Sign_Out: {
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
        // ... 其它 case ...
        default: {
            break;
        }
    }
}

void CommandHandler::handle_send(TcpServerConnection* conn) {
    conn->socket->send_with_size();
    log_info("CommandHandler::handle_send called");
}

void CommandHandler::handle_sign_in() {}

void CommandHandler::handle_sign_out() {}

void CommandHandler::handle_register(
    TcpServerConnection* conn,
    const std::string& email,
    std::string& user_ID,
    std::string& user_password) {
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
            "欢迎注册我们的聊天室(チャットルーム)服务！\n"
            "您的验证码是：" + veri_code + "\n\n"
            "请勿将此验证码分享给他人。\n"
            "此验证码5分钟后失效。\n\n"
            "感谢您的支持！\n"
            "聊天室团队"
        );
        if (sender.send()) {
            log_info("=== 邮件发送成功！===");
            // 将验证码存入Redis
            if (disp->redis_con->set_veri_code(subj, veri_code)) {
                log_info("验证码已存入Redis");
                // 发送成功，通知客户端
                auto env_out = create_command_string(
                    Action::Accept, subj, {});
                conn->socket->set_write_buf(env_out);
                conn->write_event->add_to_reactor();
            } else {
                log_error("!!! 无法存储验证码到Redis !!!");
            }
        } else {
            log_error("!!! 邮件发送失败: " + sender.get_error() + " !!!");
        }
    } catch (const std::exception& e) {
        std::cerr << "初始化错误: " << e.what() << std::endl;
        return;
    }
    auto env_out = create_command_string(
        Action::Refuse, subj, {"暂时无法使用验证服务"});
    conn->socket->set_write_buf(env_out);
    conn->write_event->add_to_reactor();
}

void CommandHandler::handle_find_password() {}

void CommandHandler::handle_change_password() {}

void CommandHandler::handle_change_username() {}

void CommandHandler::handle_authentication(
    TcpServerConnection* conn,
    const std::string& email,
    const std::string& veri_code) {
    std::string msg;
    // redis
    bool suc;
    if (suc) {
        // 可以注册
        auto env_out = create_command_string(
            Action::Accept, email, {"身份验证成功"});
        conn->socket->set_write_buf(env_out);
        conn->write_event->add_to_reactor();
    } else {
        // 不能注册
        auto env_out = create_command_string(
            Action::Refuse, email, {"验证码错误"});
        conn->socket->set_write_buf(env_out);
        conn->write_event->add_to_reactor();
    }
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

void CommandHandler::handle_get_relation_net() {}

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
