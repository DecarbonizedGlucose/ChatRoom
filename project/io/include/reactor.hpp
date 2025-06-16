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
#include "Socket.hpp"
#include <memory>
#include <mutex> // 升级更安全

class reactor;
template<typename Ret = void>
class event;

class reactor {
private:
    int epoll_fd = -1;
public:
    friend class event;
    std::unordered_map<int, event<std::any>*> events;
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
    bool add_event(event<std::any>* ev);
    bool remove_event(event<std::any>* ev);

    int get_epoll_fd() const;
};

enum class event_state {
    Running,
    Free
};

template<typename Ret = std::any>
class event {
private:
    pSocket socket = nullptr;
    bool in_reactor = false;
    bool binded = false;
    std::mutex mtx;

public:
    reactor* pr = nullptr;
    int events = 0; // EPOLLIN, EPOLLOUT, etc.
    std::function<Ret()> call_back_func = nullptr;

    std::any data;

    event() = delete;
    event(std::shared_ptr<Socket> sock, int ev, std::function<void()> cb)
        : socket(sock), events(ev), call_back_func(cb) {}
    event(std::shared_ptr<Socket> sock, int ev);
        : socket(sock), events(ev) {}
    event(const event&) = delete;
    event(event&&) = delete;
    event& operator=(const event&) = delete;
    event& operator=(event&&) = delete;
    ~event() {
        if (socket) {
            socket.reset();
        }
        if (in_reactor) {
            remove_from_reactor();
            in_reactor = false;
        }
        if (binded) {
            // 不要delete! 不要delete! 不要delete!
            // 这里的pr是reactor的智能指针的原指针
            pr = nullptr;
            binded = false;
        }
    }

    void event::set(int ev) {
        events = ev;
    }

    void set(std::function<void()> cb) {
        call_back_func = cb;
    }

    void set(int ev, std::function<void()> cb) {
        events = ev;
        call_back_func = cb;
    }

    void bind_with(reactor* re) {
        if (re == nullptr || binded || in_reactor) {
            throw std::runtime_error(std::string(__func__) + "No need to bind with reactor\n");
        }
        pr = re;
        binded = true;
        return;
    }

    void add_to_reactor() {
        std::lock_guard<std::mutex> lock(mtx);
        if (pr == nullptr || !binded || in_reactor) {
            throw std::runtime_error(std::string(__func__) + ": No condition to add event to reactor\n");
        }
        struct epoll_event ev = {0, {0}};
        ev.events = events;
        ev.data.ptr = this;
        if (epoll_ctl(pr->get_epoll_fd(), EPOLL_CTL_ADD, socket->getFd(), &ev) < 0) {
            throw std::runtime_error(std::string(__func__) + ": Failed to add event to reactor - " + strerror(errno) + '\n');
        }
        in_reactor = true;
        return;
    }

    void remove_from_reactor() {
        std::lock_guard<std::mutex> lock(mtx);
        if (pr == nullptr || !binded || !in_reactor) {
            throw std::runtime_error(std::string(__func__) + ": No condition to remove event from reactor\n");
        }
        if (epoll_ctl(pr->get_epoll_fd(), EPOLL_CTL_DEL, socket->getFd(), nullptr) < 0) {
            throw std::runtime_error(std::string(__func__) + ": Failed to remove event from reactor - " + strerror(errno) + '\n');
        }
        in_reactor = false;
        return;
    }

    void call_back() {
        remove_from_reactor();
        if (call_back_func) {
            call_back_func();
        } else {
            return;
        }
        add_to_reactor(); // 比较安全，虽然一次调用来回增删纯折磨就是了
    }

    bool is_binded() const {
        return binded;
    }

    bool in_epoll() const {
        return in_reactor;
    }

    int get_sockfd() const {
        if (socket) {
            return socket->getFd();
        }
        return -1;
    }

    pSocket get_socket() const  {
        return socket;
    }
};

using rea = reactor;

#endif