#pragma once

#include <sys/epoll.h>
#include <cstring>
#include <unistd.h>
#include <cerrno>
#include <string>
#include <unordered_map>
#include <functional>
#include <any>
#include <memory>
#include <stdexcept>

class TcpServerConnection;
class reactor;

class event {
private:
    int fd;
    bool in_reactor;
    bool binded;
    int events;

public:
    TcpServerConnection* conn;
    reactor* pr;
    std::function<void()> call_back_func;

    event(int fd, int ev, TcpServerConnection* conn, std::function<void()> cb);
    event(int fd, int ev);
    event(int fd, int ev, TcpServerConnection* conn);
    event() = delete;
    event(const event&) = delete;
    event(event&&) = delete;
    event& operator=(const event&) = delete;
    event& operator=(event&&) = delete;
    ~event();

    void set(int ev);
    void set(std::function<void()> cb);
    void set(int ev, std::function<void()> cb);
    void bind_with(reactor* re);
    void add_to_reactor();
    void remove_from_reactor();
    void call_back();
    bool is_binded() const;
    bool in_epoll() const;
    int get_sockfd() const;
    void set_sockfd(int new_fd);
};

class reactor {
private:
    int epoll_fd = -1;
    int max_events = 2048;
    int epoll_timeout = 1000;

public:
    friend class event;
    std::unordered_map<int, event*> events;
    epoll_event* epoll_events;

    reactor();
    reactor(int max_events, int timeout);
    reactor(const reactor&) = delete;
    reactor(reactor&&) = delete;
    reactor& operator=(const reactor&) = delete;
    reactor& operator=(reactor&&) = delete;
    ~reactor();

    int wait();
    bool add_event(event* ev);
    bool remove_event(event* ev);
    int get_epoll_fd() const;
};

using rea = reactor;
