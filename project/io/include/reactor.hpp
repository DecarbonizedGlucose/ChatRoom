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
#include <memory>
#include <stdexcept>

class reactor;
class event;

class reactor : public std::enable_shared_from_this<reactor> {
private:
    int epoll_fd = -1;
    int max_events = 80;
    int epoll_timeout = 1000; // 1000 ms

public:
    friend class event;
    std::unordered_map<int, event*> events;
    epoll_event* epoll_events = nullptr;

    reactor() {
        epoll_fd = epoll_create1(0);
        if (epoll_fd < 0) {
            throw std::runtime_error(std::string(__func__) + ": Failed to create epoll instance - " + strerror(errno) + '\n');
        }
        epoll_events = new epoll_event[max_events];
    }

    reactor(int max_events, int timeout)
        : max_events(max_events), epoll_timeout(timeout) {
        epoll_fd = epoll_create1(0);
        if (epoll_fd < 0) {
            throw std::runtime_error(std::string(__func__) + ": Failed to create epoll instance - " + strerror(errno) + '\n');
        }
        epoll_events = new epoll_event[max_events];
    }

    reactor(const reactor&) = delete;
    reactor(reactor&&) = delete;
    reactor& operator=(const reactor&) = delete;
    reactor& operator=(reactor&&) = delete;

    ~reactor() {
        if (epoll_fd >= 0) {
            close(epoll_fd);
            epoll_fd = -1;
        }
        if (epoll_events) {
            delete[] epoll_events;
            epoll_events = nullptr;
        }
        events.clear();
    }

    int wait() {
        int ret;
        do {
            ret = epoll_wait(epoll_fd, epoll_events, max_events, epoll_timeout);
        } while (ret < 0 && errno == EINTR);
        if (ret < 0) {
            throw std::runtime_error(std::string(__func__) + ": Epoll wait failed\n");
        }
        return ret;
    }

    bool add_event(event* ev) {
        if (ev == nullptr) {
            throw std::invalid_argument(std::string(__func__) + ": Event is nullptr\n");
        }
        if (events.size() >= max_events) {
            return false; // No space for more events
        }
        events[ev->get_sockfd()] = ev;
        ev->bind_with(shared_from_this());
        return true;
    }

    bool remove_event(event* ev) {
        if (ev == nullptr || !ev->is_binded()) {
            throw std::invalid_argument(std::string(__func__) + ": Event is nullptr or not in reactor\n");
        }
        if (ev->in_epoll()) {
            ev->remove_from_reactor();
        }
        events.erase(ev->get_sockfd());
        return true;
    }

    int get_epoll_fd() const {
        return epoll_fd;
    }
};

enum class event_state {
    Writeable,
    Readable,
    Free
};

class event {
private:
    int fd = -1;
    bool in_reactor = false;
    bool binded = false;
    int events = 0; // EPOLLIN, EPOLLOUT, etc.

public:
    std::shared_ptr<reactor> pr = nullptr;
    std::function<void()> call_back_func = nullptr;
    //std::any last_return;
    event_state state = event_state::Free;

    event(int fd, int ev, std::function<void()> cb)
        : fd(fd), events(ev), call_back_func(cb) {}

    event(int fd, int ev)
        : fd(fd), events(ev) {}

    event() = delete;
    event(const event&) = delete;
    event(event&&) = delete;
    event& operator=(const event&) = delete;
    event& operator=(event&&) = delete;

    ~event() {
        fd = -1;
        if (in_reactor) {
            remove_from_reactor();
            in_reactor = false;
        }
        if (binded) {
            pr.reset();
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

    void bind_with(const std::shared_ptr<reactor>& re) {
        if (re == nullptr || binded || in_reactor) {
            throw std::runtime_error(std::string(__func__) + "No need to bind with reactor\n");
        }
        pr = re;
        binded = true;
        return;
    }

    void add_to_reactor() {
        if (pr == nullptr || !binded || in_reactor) {
            throw std::runtime_error(std::string(__func__) + ": No condition to add event to reactor\n");
        }
        struct epoll_event ev = {0, {0}};
        ev.events = events;
        ev.data.ptr = this;
        if (epoll_ctl(pr->get_epoll_fd(), EPOLL_CTL_ADD, fd, &ev) < 0) {
            throw std::runtime_error(std::string(__func__) + ": Failed to add event to reactor - " + strerror(errno) + '\n');
        }
        in_reactor = true;
        return;
    }

    void remove_from_reactor() {
        if (pr == nullptr || !binded || !in_reactor) {
            throw std::runtime_error(std::string(__func__) + ": No condition to remove event from reactor\n");
        }
        if (epoll_ctl(pr->get_epoll_fd(), EPOLL_CTL_DEL, fd, nullptr) < 0) {
            throw std::runtime_error(std::string(__func__) + ": Failed to remove event from reactor - " + strerror(errno) + '\n');
        }
        in_reactor = false;
        return;
    }

    void call_back() {
        //remove_from_reactor();
        if (call_back_func) {
            call_back_func();
        }
        //add_to_reactor(); // 比较安全，虽然一次调用来回增删纯折磨就是了
    }

    bool is_binded() const {
        return binded;
    }

    bool in_epoll() const {
        return in_reactor;
    }

    int get_sockfd() const {
        return fd;
    }
};

using rea = reactor;

#endif