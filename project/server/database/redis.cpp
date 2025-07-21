#include "../include/redis.hpp"

RedisController::RedisController()
    : redis_conn("tcp://127.0.0.1:6379") {}

std::pair<bool, std::time_t> RedisController::get_user_status(const std::string& user_ID) {
    auto key = "chat:" + user_ID + ":status";
    auto val = redis_conn.get(key);
    if (!val) {
        return {false, 0}; // 用户不存在或离线
    }
    auto jval = json::parse(*val);
    bool online = jval["online"];
    std::time_t last_active = jval["last_active"];
    return {online, last_active};
}