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
*/

class RedisController {
public:
    using Redis = sw::redis::Redis;

    RedisController();

    // chat:<user_id>:status
    std::pair<bool, std::time_t> get_user_status(const std::string& user_ID);

private:
    Redis redis_conn; // 实际连接对象

};



#endif