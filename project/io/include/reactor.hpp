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
    uint32_t events;

public:
    TcpServerConnection* conn;
    reactor* pr;
    std::function<void()> call_back_func;

    event(int fd, uint32_t ev, TcpServerConnection* conn, std::function<void()> cb);
    event(int fd, uint32_t ev);
    event(int fd, uint32_t ev, TcpServerConnection* conn);
    event() = delete;
    event(const event&) = delete;
    event(event&&) = delete;
    event& operator=(const event&) = delete;
    event& operator=(event&&) = delete;
    ~event();

    void set(uint32_t ev);
    void set(std::function<void()> cb);
    void set(uint32_t ev, std::function<void()> cb);
    void bind_with(reactor* re);
    void add_to_reactor();
    void remove_from_reactor();
    // void modify_in_reactor();
    // void add_or_modify_in_reactor();
    void add_event_to_fd();
    void remove_event_from_fd();
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
    //std::unordered_map<int, event*> events;
    std::unordered_map<int, uint32_t> fd_events_map;
    std::unordered_map<int, std::pair<event*, event*>> fd_event_obj;
    epoll_event* epoll_events = nullptr;

    reactor();
    reactor(int max_events, int timeout);
    reactor(const reactor&) = delete;
    reactor(reactor&&) = delete;
    reactor& operator=(const reactor&) = delete;
    reactor& operator=(reactor&&) = delete;
    ~reactor();

    void add_revent(event* ev, int fd);
    void add_wevent(event* ev, int fd);
    int wait();
    int get_epoll_fd() const;
};

using rea = reactor;
