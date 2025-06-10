#include "include/TcpServer.hpp"

TS::TcpServer() {
    pr = std::make_shared<rea>();
    listen_conn = std::make_shared<TCA>();
}

TS::TcpServer(const std::string& ip, uint16_t port, sa_family_t family) {
    pr = std::make_shared<rea>();
    listen_conn = std::make_shared<TCA>(ip, port, family);
}

TS::~TcpServer() {
    listen_conn.reset();
    pr.reset();
    pool.reset();
}

int TS::get_lfd() const {
    return listen_conn ? listen_conn->getfd() : -1;
}

int TS::get_efd() const {
    return pr ? pr->get_epoll_fd() : -1;
}

void TS::set_thread_pool(std::shared_ptr<thread_pool> pool) {
    this->pool = pool;
}

void TS::listen_init() {
    if (!listen_conn->set_listen()) {
        throw std::runtime_error("Failed to connect to the listening socket\n");
    }
    // 这里不用智能指针是因为，reactor内部已经管理得当
    event* ev = new event(listen_conn, EPOLLIN, [this]() {
        this->accept_connections();
    });
    ev->bind_with(pr.get());
    if (!pr->add_event(ev)) {
        // 不用担心listen_conn被析构，因为它是在event里是shared_ptr
        delete ev;
        throw std::runtime_error("Failed to add listening event to reactor\n");
    }
    ev->add_to_reactor();
}

void TS::accept_connections(std::function<void()> cb) {
    struct sockaddr_in client_addr = {0};
    socklen_t addr_len = sizeof(client_addr);
    int cfd = accept(listen_conn->getfd(), (struct sockaddr*)&client_addr, &addr_len);
    if (cfd < 0) {
        throw std::runtime_error("Failed to accept new connection: " + std::string(strerror(errno)) + '\n');
    }
    std::shared_ptr<TCD> new_conn = std::make_shared<TCD>(cfd);
    // fcntl had set non-blocking in make shared<TCD>
    event* new_event = new event(new_conn, EPOLLIN | EPOLLONESHOT, cb);
    new_event->bind_with(pr.get());
    if (!pr->add_event(new_event)) {
        delete new_event;
        throw std::runtime_error("Failed to add new connection event to reactor\n");
    }
    new_event->add_to_reactor();
}

void TS::launch() {
    if (listen_conn->is_listening() == false) {
        listen_init();
    }
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
                event* e = static_cast<event*>(ev.data.ptr);
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

