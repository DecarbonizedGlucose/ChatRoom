#include "include/TcpServer.hpp"
#include "../global/include/logging.hpp"
#include "../../io/include/reactor.hpp"
#include "include/redis.hpp"

namespace set_addr_s {
    Addr server_addr[3];
    bool fetch_addr_from_config() {
        set_addr_s::server_addr[0] = {"127.0.0.1", 9527};
        set_addr_s::server_addr[1] = {"127.0.0.1", 9528};
        set_addr_s::server_addr[2] = {"127.0.0.1", 9529};
        log_info("模拟读取了所有socket地址{}:{}, {}:{}, {}:{}",
                 set_addr_s::server_addr[0].first, set_addr_s::server_addr[0].second,
                 set_addr_s::server_addr[1].first, set_addr_s::server_addr[1].second,
                 set_addr_s::server_addr[2].first, set_addr_s::server_addr[2].second);
        return true;
    }
}

TcpServer::TcpServer(int idx) : idx(idx) {
    pr = new reactor();
    listen_conn = new ListenSocket(
        set_addr_s::server_addr[idx].first,
        set_addr_s::server_addr[idx].second,
        true
    );
}

TcpServer::TcpServer(int idx, const std::string& ip, uint16_t port) : idx(idx) { // 这函数写的有点多余
    pr = new reactor();
    listen_conn = new ListenSocket(ip, port);
}

TcpServer::~TcpServer() {
    log_info("Tcp server {} is being destroyed", idx);
    delete listen_conn;
    delete pr;
}

int TcpServer::get_lfd() const {
    return listen_conn ? listen_conn->get_fd() : -1;
}

int TcpServer::get_efd() const {
    return pr ? pr->get_epoll_fd() : -1;
}

void TcpServer::init(thread_pool* pool, RedisController* re, Dispatcher* disp) {
    this->pool = pool;
    this->disp = disp;
    disp->add_server(this, idx);
    // 监听不用write event, 更不用创建TcpServerConnection
    if (!(listen_conn->bind() && listen_conn->listen())) {
        throw std::runtime_error("Failed to start listening on " + listen_conn->get_ip() + ":" + std::to_string(listen_conn->get_port()) + ": " + strerror(errno));
    }
    event* read_event = new event(listen_conn->get_fd(), EPOLLIN | EPOLLET);
    if (!pr->add_event(read_event)) {
        delete read_event;
        throw std::runtime_error("Failed to add listening events to reactor: " + std::string(strerror(errno)));
    }
    read_event->add_to_reactor();
}

void TcpServer::start() {
    running = true;
    // main loop
    while (running) {
        int num_ready = pr->wait();
        if (num_ready < 0) {
            if (errno == EINTR) {
                continue; // 被打断
            }
            std::cerr << "Epoll wait failed: " << strerror(errno) << '\n';
            running = false;
            break; // Exit on error
        } else if (num_ready == 0) {
            //log_info("Epoll wait timed out");
            continue; // 超时，无事发生
        }
        // 轮询
        for (int i = 0; i < num_ready; ++i) {
            auto ev = pr->epoll_events[i];
            event* event_ptr = static_cast<event*>(ev.data.ptr);
            if (event_ptr == nullptr) {
                // 自唤醒
                log_info("Epoll wakes self");
                break;
            }
            // 先拉出来listen_conn的事件
            if (event_ptr->get_sockfd() == listen_conn->get_fd()) {
                this->pool->submit([this]() {
                    this->auto_accept();
                });
                continue;
            }
            // 区分读写，分发事件
            if (ev.events & EPOLLIN) {
                // 读事件
                log_info("Reactor read event at fd {}", event_ptr->get_sockfd());
                event_ptr->remove_from_reactor();
                pool->submit([event_ptr]() {
                    event_ptr->conn->dispatcher \
                    ->dispatch_recv(event_ptr->conn);
                    event_ptr->add_to_reactor();
                });
            }
            else {
                // 写事件
                log_info("Reactor write event at fd {}", event_ptr->get_sockfd());
                event_ptr->remove_from_reactor();
                pool->submit([event_ptr]() {
                    event_ptr->conn->dispatcher \
                    ->dispatch_send(event_ptr->conn);
                });
            }
        }
    }
    log_debug("Tcp server {} main loop exited", idx);
}

void TcpServer::stop() {
    running = false;
    pr->wake();
    log_info("Tcp server {} stopped", idx);
}

void TcpServer::auto_accept() {
    auto new_sock = listen_conn->accept();
    if (!new_sock) {
        log_error("Failed to accept new connection: {}", strerror(errno));
        return;
    }
    auto conn = new TcpServerConnection(pr, disp);
    conn->socket = std::move(new_sock);
    if (idx == 1) {
        conn->set_send_type(DataType::Command);
    } else if (idx == 0) {
        conn->set_send_type(DataType::Message);
    }
    // 其他的动态设定
    event* read_event = new event(new_sock->get_fd(), EPOLLIN | EPOLLET, conn);
    event* write_event = new event(new_sock->get_fd(), EPOLLOUT | EPOLLET, conn);
    if (!pr->add_event(read_event) || !pr->add_event(write_event)) {
        delete read_event;
        delete write_event;
        delete conn;
        return;
    }
    conn->read_event = read_event;
    conn->write_event = write_event;
    read_event->add_to_reactor();
    write_event->add_to_reactor();
    //user_connections[conn->get_user_id()] = conn;
}

void TcpServer::heartbeat_monitor_loop(int interval_sec, int timeout_sec) {}