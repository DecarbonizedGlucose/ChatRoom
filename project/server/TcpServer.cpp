#include "include/TcpServer.hpp"
#include "include/callbacks.hpp"
#include <iostream>

TS::TcpServer() {
    pr = std::make_shared<rea>();
    listen_conn = std::make_shared<LSocket>();
}

TS::TcpServer(const std::string& ip, uint16_t port, sa_family_t family) {
    pr = std::make_shared<rea>();
    listen_conn = std::make_shared<LSocket>(ip, port, family);
}

TS::~TcpServer() {
    listen_conn.reset();
    pr.reset();
    pool.reset();
}

int TS::get_lfd() const {
    return listen_conn ? listen_conn->getFd() : -1;
}

int TS::get_efd() const {
    return pr ? pr->get_epoll_fd() : -1;
}

void TS::set_thread_pool(std::shared_ptr<thread_pool> pool) {
    this->pool = pool;
}

bool TS::listen_init(void (*first_func)(event<>*)) {
    if (!listen_conn->listen()) {
        std::cerr << "Failed to start listening on " << listen_conn->getFd() << ": " << strerror(errno) << '\n';
        return false;
    }
    // 这里不用智能指针是因为，reactor内部已经管理得当
    event<>* ev = new event(listen_conn, EPOLLIN | EPOLLET);
    std::function<void()> cb = std::bind(first_func, ev);
    ev->bind_with(pr.get());
    if (!pr->add_event(ev)) {
        // 不用担心listen_conn被析构，因为它是在event里是shared_ptr
        delete ev;
        std::cerr << "Failed to add listening event to reactor: " << strerror(errno) << '\n';
        return false;
    }
    ev->add_to_reactor();
    return true;
}

void TS::accept_connections(std::function<void()> cb) {
    std::shared_ptr<ASocket> new_conn = listen_conn->accept();
    // fcntl had set non-blocking in make shared<TCD>
    event<>* new_event = new event(new_conn, EPOLLIN | EPOLLONESHOT, cb);
    new_event->bind_with(pr.get());
    if (!pr->add_event(new_event)) {
        delete new_event;
        throw std::runtime_error("Failed to add new connection event to reactor\n");
    }
    new_event->add_to_reactor();
}

void TS::launch() {
    running = true;
    int nready;
    while (running) {
        nready = pr->wait();
        if (nready < 0) {
            if (errno == EINTR) {
                continue; // 被信号打断，继续等待
            }
            throw std::runtime_error("Epoll wait failed: " + std::string(strerror(errno)) + '\n');
        } else if (nready == 0) {
            continue; // 超时，没有事件发生
        }
        for (int i = 0; i < nready; ++i) {
            auto ev = pr->epoll_events[i];
            if (ev.events & EPOLLIN || ev.events & EPOLLOUT) {
                event<>* e = static_cast<event<>*>(ev.data.ptr);
                if (pool) {
                    pool->submit([e]() {
                        e->call_back();
                    });
                } else {
                    e->call_back();
                }
            }
        }
    }
}

void TS::stop() {
    running = false;
}

void TS::transfer_content(const std::string& user_ID, const MesPtr& message) {
    event<>* ev = conn_manager->get_user_event(user_ID);
    // 再封装一层吧
}

void TS::transfer_content(const std::string& user_ID, const ComPtr& command) {
}