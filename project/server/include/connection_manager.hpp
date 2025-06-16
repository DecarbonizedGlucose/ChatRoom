#ifndef CONN_MANA_HPP
#define CONN_MANA_HPP

#include "../../io/include/Socket.hpp"
#include "../../io/include/reactor.hpp"
#include <string>
#include <unordered_map>

class ConnectionManager {
private:
    // 只做映射，不做内存管理
    std::unordered_map<std::string, event<>*> user_events; // user_ID -> event

public:
    void add_user(std::string user_ID, event<>* ev);
    void remove_user(const std::string& user_ID);
    void remove_user(event<>* ev);
    event<>* get_user_event(const std::string& user_ID) const;
    bool user_exists(const std::string& user_ID) const;
};

using CM = ConnectionManager;
using UserEventMap = std::unordered_map<std::string, event<>*>;

#endif