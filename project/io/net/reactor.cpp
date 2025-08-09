#include "../include/reactor.hpp"
#include "../../global/include/logging.hpp"
#include <sys/socket.h>
#include <fcntl.h>

event::event(int fd, uint32_t ev, TcpServerConnection* conn, std::function<void()> cb)
    : fd(fd), events(ev), conn(conn), call_back_func(cb), in_reactor(false), binded(false), pr(nullptr) {}

event::event(int fd, uint32_t ev)
    : fd(fd), events(ev), in_reactor(false), binded(false), pr(nullptr) {}

event::event(int fd, uint32_t ev, TcpServerConnection* conn)
    : fd(fd), events(ev), conn(conn), in_reactor(false), binded(false), pr(nullptr) {}

event::~event() {
    fd = -1;
    if (in_reactor) {
        remove_from_reactor();
        in_reactor = false;
    }
    if (binded) {
        pr = nullptr;
        binded = false;
    }
}

void event::set(uint32_t ev) { events = ev; }
void event::set(std::function<void()> cb) { call_back_func = cb; }
void event::set(uint32_t ev, std::function<void()> cb) { events = ev; call_back_func = cb; }

void event::bind_with(reactor* re) {
    if (re == nullptr || binded || in_reactor) {
        throw std::runtime_error(std::string(__func__) + " No need to bind with reactor\n");
    }
    pr = re;
    binded = true;
}

void event::add_to_reactor() {
    add_event_to_fd();
    in_reactor = true;
}

void event::remove_from_reactor() {
    remove_event_from_fd();
    in_reactor = false;
}

void event::add_event_to_fd() {
    if (pr->fd_events_map.find(fd) != pr->fd_events_map.end()) {
        if (pr->fd_events_map[fd] & events) {
            return;
        }
        struct epoll_event ev = {0, {0}};
        ev.events = pr->fd_events_map[fd] | events;
        ev.data.fd = fd;
        uint32_t new_events = ev.events;  // 复制到临时变量
        log_info("MOD: fd={}, old_events={:#x}, new_events={:#x}", fd, pr->fd_events_map[fd], new_events);
        int result = epoll_ctl(pr->epoll_fd, EPOLL_CTL_MOD, fd, &ev);
        if (result < 0) {
            log_error("epoll_ctl MOD failed for fd={}: {}", fd, strerror(errno));
        }
        pr->fd_events_map[fd] = ev.events;
    } else {
        pr->fd_events_map[fd] = events;
        struct epoll_event ev = {0, {0}};
        ev.events = events;
        ev.data.fd = fd;
        int result = epoll_ctl(pr->epoll_fd, EPOLL_CTL_ADD, fd, &ev);
        if (result < 0) {
            log_error("epoll_ctl ADD failed for fd={}: {}", fd, strerror(errno));
        } else if (events & EPOLLOUT) {
            log_info("Write event registered for fd={}, waiting for trigger", fd);
        }
    }
}

void event::remove_event_from_fd() {
    if (pr->fd_events_map.find(fd) == pr->fd_events_map.end()) {
        return; // 没有事件绑定
    }
    if (pr->fd_events_map[fd] & events) {
        pr->fd_events_map[fd] &= ~events; // 移除事件
        if (pr->fd_events_map[fd] == 0) {
            pr->fd_events_map.erase(fd); // 如果没有事件了, 删除映射
            epoll_ctl(pr->epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
        } else {
            struct epoll_event ev = {0, {0}};
            ev.events = pr->fd_events_map[fd];
            ev.data.fd = fd;
            epoll_ctl(pr->epoll_fd, EPOLL_CTL_MOD, fd, &ev);
        }
    }
}

void event::call_back() {
    if (call_back_func) {
        call_back_func();
    }
}

bool event::is_binded() const { return binded; }
bool event::in_epoll() const { return in_reactor; }
int event::get_sockfd() const { return fd; }
void event::set_sockfd(int new_fd) { fd = new_fd; }

reactor::reactor() {
    epoll_fd = epoll_create1(0);
    epoll_events = new epoll_event[max_events];
}

reactor::reactor(int max_events, int timeout)
    : max_events(max_events), epoll_timeout(timeout), epoll_fd(-1), epoll_events(nullptr) {
    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        throw std::runtime_error(std::string(__func__) + ": Failed to create epoll instance - " + strerror(errno) + '\n');
    }
    epoll_events = new epoll_event[max_events];
}

reactor::~reactor() {
    if (epoll_fd >= 0) {
        close(epoll_fd);
        epoll_fd = -1;
    }
    if (epoll_events) {
        delete[] epoll_events;
        epoll_events = nullptr;
    }
}

void reactor::add_revent(event* ev, int fd) {
    if (ev == nullptr || fd < 0) {
        throw std::runtime_error(std::string(__func__) + ": Invalid event or file descriptor\n");
    }
    fd_event_obj[fd].first = ev;
}

void reactor::add_wevent(event* ev, int fd) {
    if (ev == nullptr || fd < 0) {
        throw std::runtime_error(std::string(__func__) + ": Invalid event or file descriptor\n");
    }
    fd_event_obj[fd].second = ev;
}

int reactor::wait() {
    int ret;
    do {
        ret = epoll_wait(epoll_fd, epoll_events, max_events, epoll_timeout);
    } while (ret < 0 && errno == EINTR);
    if (ret < 0) {
        throw std::runtime_error(std::string(__func__) + ": Epoll wait failed\n");
    }
    return ret;
}

int reactor::get_epoll_fd() const {
    return epoll_fd;
}