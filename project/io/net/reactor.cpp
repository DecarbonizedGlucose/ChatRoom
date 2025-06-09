#include "../include/reactor.hpp"
#include <stdexcept>

/* ---------- event ---------- */

event::event(Socket* sock, int ev, std::function<void()> cb)
    : socket(sock), events(ev), call_back_func(cb) {}

event::event(int ev) : events(ev) {}

event::~event() {
    if (socket) {
        delete socket;
        socket = nullptr;
    }
    if (in_reactor) {
        remove_from_reactor();
        in_reactor = false;
    }
    if (binded) {
        pr = nullptr;
        binded = false;
    }
}

void event::set(int ev) {
    events = ev;
}

void event::set(std::function<void()> cb) {
    call_back_func = cb;
}

void event::set(int ev, std::function<void()> cb) {
    events = ev;
    call_back_func = cb;
}

void event::bind_with(reactor* rea) {
    if (rea == nullptr || binded || in_reactor) {
        throw std::runtime_error (std::string(__func__) + "No need to bind with reactor\n");
    }
    pr = rea;
    binded = true;
    return;
}

void event::add_to_reactor() {
    if (pr == nullptr || !binded || in_reactor) {
        throw std::runtime_error(std::string(__func__) + ": No condition to add event to reactor\n");
    }
    struct epoll_event ev = {0, {0}};
    ev.events = events;
    ev.data.ptr = this;
    if (epoll_ctl(pr->get_epoll_fd(), EPOLL_CTL_ADD, socket->fd, &ev) < 0) {
        throw std::runtime_error(std::string(__func__) + ": Failed to add event to reactor - " + strerror(errno) + '\n');
    }
    in_reactor = true;
    return;
}

void event::remove_from_reactor() {
    if (pr == nullptr || !binded || !in_reactor) {
        throw std::runtime_error(std::string(__func__) + ": No condition to remove event from reactor\n");
    }
    if (epoll_ctl(pr->get_epoll_fd(), EPOLL_CTL_DEL, socket->fd, nullptr) < 0) {
        throw std::runtime_error(std::string(__func__) + ": Failed to remove event from reactor - " + strerror(errno) + '\n');
    }
    in_reactor = false;
    return;
}

bool event::is_binded() const {
    return binded;
}

bool event::in_epoll() const {
    return in_reactor;
}

void event::call_back() {
    if (call_back_func) {
        call_back_func();
    } else {
        return;
    }
}

/* ---------- reactor ---------- */

rea::reactor() {
    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        throw std::runtime_error(std::string(__func__) + ": Failed to create epoll instance - " + strerror(errno) + '\n');
    }
    epoll_events = new epoll_event[max_events];
}

rea::reactor(int max_events, int timeout)
    : max_events(max_events), epoll_timeout(timeout) {
    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        throw std::runtime_error(std::string(__func__) + ": Failed to create epoll instance - " + strerror(errno) + '\n');
    }
    epoll_events = new epoll_event[max_events];
}

rea::~reactor() {
    if (epoll_fd >= 0) {
        close(epoll_fd);
        epoll_fd = -1;
    }
    if (epoll_events) {
        delete[] epoll_events;
        epoll_events = nullptr;
    }
    for (auto& pair : events) {
        delete pair.second;
    }
    events.clear();
}

int rea::wait() {
    int ret;
    do {
        ret = epoll_wait(epoll_fd, epoll_events, max_events, epoll_timeout);
    } while (ret < 0 && errno == EINTR);
    if (ret < 0) {
        throw std::runtime_error(std::string(__func__) + ": Epoll wait failed\n");
    }
    return ret;
}

bool rea::add_event(event* ev) {
    if (ev == nullptr) {
        throw std::invalid_argument(std::string(__func__) + ": Event is nullptr\n");
    }
    if (events.size() >= max_events) {
        return false; // No space for more events
    }
    events[ev->get_sockfd()] = ev;
    ev->bind_with(this);
    return true;
}

bool rea::remove_event(event* ev) {
    if (ev == nullptr || !ev->is_binded()) {
        throw std::invalid_argument(std::string(__func__) + ": Event is nullptr or not in reactor\n");
    }
    if (ev->in_epoll()) {
        ev->remove_from_reactor();
    }
    events.erase(ev->get_sockfd());
    return true;
}

