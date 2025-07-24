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

void RedisController::set_user_status(const std::string& user_ID, bool online) {
    auto key = "chat:" + user_ID + ":status";
    json jval = {
        {"online", online},
        {"last_active", std::time(nullptr)}
    };
    redis_conn.set(key, jval.dump());
}

void RedisController::set_veri_code(const std::string& user_email, const std::string& veri_code) {
    auto key = "chat:" + user_email + ":veri_code";
    redis_conn.setex(key, std::chrono::seconds(300), veri_code);
}

std::string RedisController::get_veri_code(const std::string& user_email) {
    auto key = "chat:" + user_email + ":veri_code";
    auto val = redis_conn.get(key);
    if (!val) {
        return ""; // 用户不存在或验证码未设置
    }
    return *val;
}

void RedisController::del_veri_code(const std::string& user_email) {
    auto key = "chat:" + user_email + ":veri_code";
    redis_conn.del(key);
}
