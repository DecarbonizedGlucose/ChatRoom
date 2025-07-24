#ifndef REDIS_HPP
#define REDIS_HPP

#include <sw/redis++/redis++.h>
#include <string>
#include <utility>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

/*
 * redis.hpp
 * 这个文件本来可以不存在
 * 只是为了为了让代码逻辑清晰
*/

/*
用户在线状态：
chat:<user_id>:status -> {
    "online" : true/false,
    "last_active" : <timestamp> // 用于心跳检测
}

用户验证码：
chat:<user_email>:veri_code -> "114514" // 6位数字验证码
*/

class RedisController {
public:
    using Redis = sw::redis::Redis;

    RedisController();

/* ---------- 用户系统 ---------- */

    // chat:<user_id>:status
    std::pair<bool, std::time_t> get_user_status(const std::string& user_ID);
    void set_user_status(const std::string& user_ID, bool online);
    // chat:<user_email>:veri_code
    void set_veri_code(const std::string& user_email, const std::string& veri_code);
    std::string get_veri_code(const std::string& user_email);
    void del_veri_code(const std::string& user_email);

/* ---------- 在线消息 ---------- */

private:
    Redis redis_conn; // 实际连接对象
};



#endif