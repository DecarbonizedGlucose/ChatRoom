#include "../include/connection_manager.hpp"

void CM::add_user(std::string user_ID, event<>* ev) {
    user_events[std::move(user_ID)] = ev;
}

void CM::remove_user(const std::string& user_ID) {
    auto it = user_events.find(user_ID);
    if (it != user_events.end()) {
        user_events.erase(it);
    }
}

void CM::remove_user(event<>* ev) {
    for (auto it = user_events.begin(); it != user_events.end(); ++it) {
        if (it->second == ev) {
            user_events.erase(it);
            return;
        }
    }
}

event<>* CM::get_user_event(const std::string& user_ID) const {
    auto it = user_events.find(user_ID);
    if (it != user_events.end()) {
        return it->second;
    }
    return nullptr;
}

bool CM::user_exists(const std::string& user_ID) const {
    return user_events.find(user_ID) != user_events.end();
}