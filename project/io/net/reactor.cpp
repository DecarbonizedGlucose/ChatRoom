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
    // log_debug("add_to_reactor called: fd={}, events={:#x}", fd, events);
    add_event_to_fd();
    in_reactor = true;
    // log_debug("add_to_reactor completed: fd={}, in_reactor={}", fd, in_reactor);
}

void event::remove_from_reactor() {
    remove_event_from_fd();
    in_reactor = false;
}

// void event::modify_in_reactor() {
//     if (pr == nullptr || !binded || !in_reactor) {
//         throw std::runtime_error(std::string(__func__) + ": No condition to modify event in reactor\n");
//     }
//     struct epoll_event ev = {0, {0}};
//     ev.events = events;
//     ev.data.ptr = this;
//     if (epoll_ctl(pr->get_epoll_fd(), EPOLL_CTL_MOD, fd, &ev) < 0) {
//         throw std::runtime_error(std::string(__func__) + ": Failed to modify event in reactor - " + strerror(errno));
//     }
// }

// void event::add_or_modify_in_reactor() {
//     if (pr == nullptr || !binded) {
//         throw std::runtime_error(std::string(__func__) + ": No condition to add/modify event in reactor\n");
//     }
//     struct epoll_event ev = {0, {0}};
//     ev.events = events;
//     ev.data.ptr = this;

//     if (in_reactor) {
//         // 如果已经在 reactor 中, 使用 MOD
//         if (epoll_ctl(pr->get_epoll_fd(), EPOLL_CTL_MOD, fd, &ev) < 0) {
//             throw std::runtime_error(std::string(__func__) + ": Failed to modify event in reactor - " + strerror(errno));
//         }
//     } else {
//         // 如果不在 reactor 中, 使用 ADD
//         if (epoll_ctl(pr->get_epoll_fd(), EPOLL_CTL_ADD, fd, &ev) < 0) {
//             throw std::runtime_error(std::string(__func__) + ": Failed to add event to reactor - " + strerror(errno));
//         }
//         in_reactor = true;
//     }
// }

void event::add_event_to_fd() {
    // log_debug("add_event_to_fd called: fd={}, events={:#x} (signed: {})", fd, (uint32_t)events, events);

    // // 如果是写事件, 检查 socket 状态
    // if (events & EPOLLOUT) {
    //     // 检查 socket 是否有效
    //     int error = 0;
    //     socklen_t len = sizeof(error);
    //     int result = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);
    //     if (result == 0) {
    //         if (error != 0) {
    //             log_error("Socket fd={} has error: {}", fd, strerror(error));
    //         } else {
    //             log_debug("Write event: Socket fd={} status OK", fd);
    //         }
    //     } else {
    //         log_error("Failed to get socket status for fd={}: {}", fd, strerror(errno));
    //     }
    // }

    if (pr->fd_events_map.find(fd) != pr->fd_events_map.end()) {
        if (pr->fd_events_map[fd] & events) {
            // log_debug("Event already exists for fd={}, events={:#x} (signed: {})", fd, (uint32_t)events, events);
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
        uint32_t add_events = ev.events;  // 复制到临时变量
        //log_debug("ADD: fd={}, events={:#x}", fd, add_events);
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
    // log_debug("reactor 析构: this={}, epoll_event={}", (void*)this, (void*)epoll_events);
    if (epoll_fd >= 0) {
        close(epoll_fd);
        epoll_fd = -1;
    }
    if (epoll_events) {
        delete[] epoll_events;
        epoll_events = nullptr;
    }
    //events.clear();
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
        // log_debug("reactor::wait : epoll_fd={}, epoll_events={}, max_events={}, epoll_timeout={}",
        //           epoll_fd, static_cast<const void*>(epoll_events), max_events, epoll_timeout);
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