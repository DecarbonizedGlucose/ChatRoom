#include "../include/reactor.hpp"

event::event(int fd, int ev, TcpServerConnection* conn, std::function<void()> cb)
    : fd(fd), events(ev), conn(conn), call_back_func(cb), in_reactor(false), binded(false), pr(nullptr) {}

event::event(int fd, int ev)
    : fd(fd), events(ev), in_reactor(false), binded(false), pr(nullptr) {}

event::event(int fd, int ev, TcpServerConnection* conn)
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

void event::set(int ev) { events = ev; }
void event::set(std::function<void()> cb) { call_back_func = cb; }
void event::set(int ev, std::function<void()> cb) { events = ev; call_back_func = cb; }

void event::bind_with(reactor* re) {
    if (re == nullptr || binded || in_reactor) {
        throw std::runtime_error(std::string(__func__) + " No need to bind with reactor\n");
    }
    pr = re;
    binded = true;
}

void event::add_to_reactor() {
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
}

void event::remove_from_reactor() {
    if (pr == nullptr || !binded || !in_reactor) {
        throw std::runtime_error(std::string(__func__) + ": No condition to remove event from reactor\n");
    }
    if (epoll_ctl(pr->get_epoll_fd(), EPOLL_CTL_DEL, fd, nullptr) < 0) {
        throw std::runtime_error(std::string(__func__) + ": Failed to remove event from reactor - " + strerror(errno) + '\n');
    }
    in_reactor = false;
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
    if (epoll_fd < 0) {
        throw std::runtime_error(std::string(__func__) + ": Failed to create epoll instance - " + strerror(errno) + '\n');
    }
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
    events.clear();
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

bool reactor::add_event(event* ev) {
    if (ev == nullptr) {
        throw std::invalid_argument(std::string(__func__) + ": Event is nullptr\n");
    }
    if (events.size() >= max_events) {
        return false;
    }
    events[ev->get_sockfd()] = ev;
    ev->bind_with(this);
    return true;
}

bool reactor::remove_event(event* ev) {
    if (ev == nullptr || !ev->is_binded()) {
        throw std::invalid_argument(std::string(__func__) + ": Event is nullptr or not in reactor\n");
    }
    if (ev->in_epoll()) {
        ev->remove_from_reactor();
    }
    events.erase(ev->get_sockfd());
    return true;
}

int reactor::get_epoll_fd() const {
    return epoll_fd;
}