#include "../include/dispatcher.hpp"
#include "../include/TcpServer.hpp"
#include "handler.hpp"
#include "../../global/include/logging.hpp"
#include "../include/connection_manager.hpp"
#include "sfile_manager.hpp"
// #include "../../global/abstract/datatypes.hpp"

using RecvState = DataSocket::RecvState;

Dispatcher::Dispatcher(RedisController* re, MySQLController* my)
    : redis_con(re), mysql_con(my) {
    message_handler = new MessageHandler(this);
    command_handler = new CommandHandler(this);
    file_handler = new FileHandler(this);
    sync_handler = new SyncHandler(this);
    offline_message_handler = new OfflineMessageHandler(this);
    conn_manager = new ConnectionManager(this);
    file_manager = new SFileManager(this);
}

Dispatcher::~Dispatcher() {
    delete message_handler;
    delete command_handler;
    delete file_handler;
    delete sync_handler;
    delete offline_message_handler;
    delete file_manager;
}

void Dispatcher::add_server(TcpServer* server, int idx) {
    if (idx < 0 || idx >= 3) {
        throw std::out_of_range("Index out of range for server array");
    }
    this->server[idx] = server;
}

// void Dispatcher::flush_cached_messages() {
//     log_info("Flushing cached messages to MySQL");
//     server[0]->pool->submit([&](){
//         size_t batch_size = 200;
//         auto batch = redis_con->pop_chat_messages_batch(batch_size);
//         if (batch.empty()) return;
//         for (auto &raw : batch) {
//             auto msg = get_chat_message(raw);
//             // 存起来
//             mysql_con->add_chat_message(
//                 msg.sender(), msg.receiver(), msg.is_group(), msg.timestamp(),
//                 msg.text(), msg.pin(), msg.payload().file_name(),
//                 msg.payload().file_size(), msg.payload().file_hash()
//             );
//         }
//     });
// }

void Dispatcher::dispatch_recv(TcpServerConnection* conn) {
    log_debug("dispatch_recv called for connection fd: {}", conn->socket->get_fd());
    std::string proto_str;
    // 读
    while (1) {
        RecvState state = conn->socket->receive_protocol_with_state(proto_str);
        if (state == RecvState::NoMoreData) {
            //log_debug("No more data available for fd: {}", conn->socket->get_fd());
            break;
        } else if (state == RecvState::Disconnected) {
            log_info("Connection fd {} disconnected", conn->socket->get_fd());
            if (conn->user_ID.empty()) {
                // 未登录用户, 直接删除连接
                delete conn;
                conn = nullptr;
            } else {
                // 已登录用户, 先保存user_ID, 然后执行登出处理
                std::string user_id = conn->user_ID;
                log_debug("Processing sign out for user: {}", user_id);

                // 注意：handle_sign_out 内部会调用 remove_user, 这会删除所有该用户的连接
                // 包括当前的 conn 对象, 所以之后不能再使用 conn
                command_handler->handle_uncommon_disconnect(user_id); // 非正常断开处理
                conn = nullptr;
            }
            return; // 连接断开, 退出处理循环
        } else if (state == RecvState::Error) {
            log_error("Error receiving data from connection (fd: {})", conn->socket->get_fd());
            return; // 处理错误
        }
        //log_debug("Received data from connection (fd:{})", conn->socket->get_fd());

        // 收到数据, 更新发送的用户活动时间
        if (!conn->user_ID.empty())
            conn_manager->update_user_activity(conn->user_ID);

        // 转写
        Envelope env;
        if (!env.ParseFromString(proto_str)) {
            log_error("Failed to parse Envelope from received data");
            break;
        }
        const google::protobuf::Any& any = env.payload();
        //std::cout << "分发器测试类型[" << any.type_url() << "]" << std::endl;
        if (any.Is<ChatMessage>()) {
            ChatMessage chat_msg;
            any.UnpackTo(&chat_msg);
            // 消息接收
            message_handler->handle_recv(chat_msg, proto_str);
            // // 定时批量存储
            // auto now = std::chrono::steady_clock::now();
            // auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_flush_time);

            // if (elapsed >= flush_interval) {
            //     flush_cached_messages();
            //     last_flush_time = now;
            // }
        } else if (any.Is<CommandRequest>()) {
            CommandRequest cmd_req;
            any.UnpackTo(&cmd_req);
            // 命令单向发送
            command_handler->handle_recv(conn, cmd_req, proto_str);
        } else if (any.Is<FileChunk>()) {
            FileChunk file_chunk;
            any.UnpackTo(&file_chunk);
            // 文件分片
            file_handler->handle_recv(conn, file_chunk);
        } else { // 剩下的不用服务器收
            log_error("Unknown payload type");
            continue;
        }
    }
    //log_debug("Attempting to re-add read event for fd: {}", conn->socket->get_fd());

    // 添加安全检查
    if (!conn->read_event) {
        log_error("CRITICAL: conn->read_event is null for fd: {}", conn->socket->get_fd());
        return;
    }

    //log_debug("read_event is valid, calling add_to_reactor()");
    try {
        conn->read_event->add_to_reactor();
        //log_debug("Read event added/modified in reactor (fd:{})", conn->read_event->get_sockfd());
    } catch (const std::exception& e) {
        log_error("Exception in add_to_reactor() for fd {}: {}", conn->socket->get_fd(), e.what());
    } catch (...) {
        log_error("Unknown exception in add_to_reactor() for fd {}", conn->socket->get_fd());
    }
}

void Dispatcher::dispatch_send(TcpServerConnection* conn) {
    switch (conn->to_send_type) {
        case DataType::Message: {
            message_handler->handle_send(conn);
            break;
        }
        case DataType::Command: {
            command_handler->handle_send(conn);
            break;
        }
        case DataType::FileChunk: {
            // 不知道
            break;
        }
        case DataType::SyncItem: {
            break;
        }
        case DataType::OfflineMessages: {
            break;
        }
        default: {
            std::cerr << "Unknown data type for sending\n";
            return;
        }
    }

    // 统一处理：发送完成后, 重新添加读事件以继续接收数据
    //log_debug("dispatch_send: Attempting to re-add read event for fd: {}", conn->socket->get_fd());

    if (!conn->read_event) {
        log_error("dispatch_send: CRITICAL: conn->read_event is null for fd: {}", conn->socket->get_fd());
        return;
    }

    //log_debug("dispatch_send: read_event is valid, calling add_to_reactor()");
    try {
        conn->read_event->add_to_reactor();
        //log_debug("dispatch_send: Read event re-added/modified in reactor (fd:{})", conn->read_event->get_sockfd());
    } catch (const std::exception& e) {
        log_error("dispatch_send: Exception in add_to_reactor() for fd {}: {}", conn->socket->get_fd(), e.what());
    } catch (...) {
        log_error("dispatch_send: Unknown exception in add_to_reactor() for fd {}", conn->socket->get_fd());
    }
}