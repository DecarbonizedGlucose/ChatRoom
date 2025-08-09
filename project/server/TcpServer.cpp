#include "include/TcpServer.hpp"
#include "../global/include/logging.hpp"
#include "../../io/include/reactor.hpp"
#include "include/redis.hpp"
#include "../io/include/Socket.hpp"
#include "include/dispatcher.hpp"

namespace set_addr_s {
    Addr server_addr[3];
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

void TcpServer::init(thread_pool* pool, Dispatcher* disp) {
    this->pool = pool;
    this->disp = disp;
    disp->add_server(this, idx);
    // 监听不用write event, 更不用创建TcpServerConnection
    if (!(listen_conn->bind() && listen_conn->listen())) {
        throw std::runtime_error("Failed to start listening on " + listen_conn->get_ip() + ":" + std::to_string(listen_conn->get_port()) + ": " + strerror(errno));
    }
    event* read_event = new event(listen_conn->get_fd(), EPOLLIN | EPOLLET);
    read_event->bind_with(pr);
    pr->add_revent(read_event, listen_conn->get_fd());
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
            log_error(std::string("Epoll wait failed: ") + strerror(errno));
            running = false;
            break; // Exit on error
        } else if (num_ready == 0) {
            //log_info("Epoll wait timed out");
            continue; // 超时, 无事发生
        }
        // 轮询
        for (int i = 0; i < num_ready; ++i) {
            auto ev = pr->epoll_events[i];
            int fd = ev.data.fd;
            event* read_event = pr->fd_event_obj[fd].first;
            event* write_event = pr->fd_event_obj[fd].second;
            // 先拉出来listen_conn的事件
            if (fd == listen_conn->get_fd()) {
                this->pool->submit([this]() {
                    this->auto_accept();
                });
                continue;
            }
            // 区分读写, 分发事件
            if (ev.events & EPOLLIN) {
                // 读事件
                log_info("Reactor read event at fd {}", fd);
                read_event->remove_from_reactor();
                //log_debug("Read event removed from reactor (fd:{})", fd);
                pool->submit([read_event]() {
                    read_event->conn->dispatcher \
                    ->dispatch_recv(read_event->conn);
                });
            }
            if (ev.events & EPOLLOUT) {
                // 写事件
                log_info("Reactor write event at fd {}", fd);
                write_event->remove_from_reactor();
                //log_debug("Write event removed from reactor (fd:{})", fd);
                pool->submit([write_event]() {
                    write_event->conn->dispatcher \
                    ->dispatch_send(write_event->conn);
                });
            }
        }
    }
    //log_debug("Tcp server {} main loop exited", idx);
}

void TcpServer::stop() {
    running = false;
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
    read_event->bind_with(pr);
    write_event->bind_with(pr);
    conn->read_event = read_event;
    conn->write_event = write_event;
    pr->add_revent(read_event, new_sock->get_fd());
    pr->add_wevent(write_event, new_sock->get_fd());
    read_event->add_to_reactor();
    write_event->add_to_reactor();
}