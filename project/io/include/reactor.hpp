#ifndef REACTOR_HPP
#define REACTOR_HPP

#include <sys/epoll.h>
#include <cstring>
#include <unistd.h>
#include <cerrno>
#include <string>
#include <unordered_map>
#include <functional>
#include <any>
#include "TcpConnection.hpp"

class reactor;
class event;

class reactor {
private:
    int epoll_fd = -1;
public:
    friend class event;
    std::unordered_map<int, event*> events;
    struct epoll_event* epoll_events = nullptr;
    int max_events = 80;
    int epoll_timeout = 1000; // 1000 ms

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

class event {
private:
    TC* tc = nullptr;
    bool in_reactor = false;
    bool binded = false;
public:
    reactor* pr = nullptr;
    int events = 0; // EPOLLIN, EPOLLOUT, etc.
    std::function<void()> call_back_func = nullptr;

    std::any data;

    event() = delete;
    event(TcpConnection* sock, int ev, std::function<void()> cb);
    event(int ev);
    event(const event&) = delete;
    event(event&&) = delete;
    event& operator=(const event&) = delete;
    event& operator=(event&&) = delete;
    ~event();

    void set(int ev);
    void set(std::function<void()> cb);
    void set(int ev, std::function<void()> cb);
    void bind_with(reactor* rea);
    void add_to_reactor();
    void remove_from_reactor();
    void call_back();

    bool is_binded() const;
    bool in_epoll() const;
    int get_sockfd() const;
};

using rea = reactor;

#endif