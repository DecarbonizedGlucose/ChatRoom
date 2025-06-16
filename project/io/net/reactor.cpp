#include "../include/reactor.hpp"
#include <stdexcept>

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

bool rea::add_event(event<std::any>* ev) {
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

bool rea::remove_event(event<std::any>* ev) {
    if (ev == nullptr || !ev->is_binded()) {
        throw std::invalid_argument(std::string(__func__) + ": Event is nullptr or not in reactor\n");
    }
    if (ev->in_epoll()) {
        ev->remove_from_reactor();
    }
    events.erase(ev->get_sockfd());
    return true;
}

