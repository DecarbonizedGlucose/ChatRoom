#include "include/TcpServer.hpp"
#include <iostream>

TcpServer::TcpServer(int idx) : idx(idx) {
    pr = std::make_shared<reactor>();
    listen_conn = std::make_shared<ListenSocket>();
}

TcpServer::TcpServer(int idx, const std::string& ip, uint16_t port) : idx(idx) {
    pr = std::make_shared<reactor>();
    listen_conn = std::make_shared<ListenSocket>(ip, port);
}

TcpServer::~TcpServer() {
    listen_conn.reset();
    pr.reset();
    pool.reset();
}

int TcpServer::get_lfd() const {
    return listen_conn ? listen_conn->get_fd() : -1;
}

int TcpServer::get_efd() const {
    return pr ? pr->get_epoll_fd() : -1;
}

void TcpServer::init(std::shared_ptr<thread_pool> pool, RedisController* re, Dispatcher* disp) {
    this->pool = pool;
    this->disp = disp;
    disp->add_server(this, idx);
    // 监听不用write event, 更不用创建TcpServerConnection
    if (!(listen_conn->bind() && listen_conn->listen())) {
        pr.reset();
        pool.reset();
        throw std::runtime_error("Failed to start listening on " + listen_conn->get_ip() + ":" + std::to_string(listen_conn->get_port()) + ": " + strerror(errno));
    }
    event* read_event = new event(listen_conn->get_fd(), EPOLLIN | EPOLLET);
    read_event->bind_with(pr->shared_from_this());
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
            continue; // 超时，无事发生
        }
        // 轮询
        for (int i = 0; i < num_ready; ++i) {
            auto ev = pr->epoll_events[i];
            event* event_ptr = static_cast<event*>(ev.data.ptr);
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
                pool->submit([event_ptr]() {
                    event_ptr->conn->dispatcher \
                    ->dispatch_recv(event_ptr->conn);
                });
            }
            else {
                // 写事件
                pool->submit([event_ptr]() {
                    event_ptr->conn->dispatcher \
                    ->dispatch_send(event_ptr->conn);
                });
            }
        }
    }
}

void TcpServer::stop() {}

void TcpServer::auto_accept() {
    auto new_conn = listen_conn->accept();
    if (!new_conn) {
        std::cerr << "Failed to accept new connection: " << strerror(errno) << '\n';
        return;
    }
    auto conn = std::make_shared<TcpServerConnection>(pr->shared_from_this(), disp);
    event* read_event = new event(new_conn->get_fd(), EPOLLIN | EPOLLET, conn);
    read_event->bind_with(pr->shared_from_this());
    event* write_event = new event(new_conn->get_fd(), EPOLLOUT | EPOLLET, conn);
    write_event->bind_with(pr->shared_from_this());
    if (!pr->add_event(read_event) || !pr->add_event(write_event)) {
        delete read_event;
        delete write_event;
        conn.reset();
        return;
    }
    read_event->add_to_reactor();
    write_event->add_to_reactor();
    //user_connections[conn->get_user_id()] = conn;
}

void TcpServer::heartbeat_monitor_loop(int interval_sec, int timeout_sec) {}