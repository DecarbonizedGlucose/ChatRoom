#include "../include/redis.hpp"
#include "../../global/include/logging.hpp"
#include <unordered_map>
#include <unordered_set>
#include <iterator>
#include "../../global/include/time_utils.hpp"

RedisController::RedisController()
    : redis_conn("tcp://127.0.0.1:6379") {}

bool RedisController::cache_chat_message(
    const std::string& serialized_msg,
    const std::string& conv,
    int64_t timestamp
) {
    try {
        redis_conn.zadd("chat:messages:" + conv,
                        serialized_msg,
                        static_cast<double>(timestamp));
        return true;
    } catch (const sw::redis::Error &err) {
        log_error("Failed to cache chat message: {}", err.what());
        return false;
    }
}

std::vector<std::string> RedisController::pop_chat_messages_batch(size_t count) {
    std::vector<std::string> messages;
    try {
        // 用 scan 扫描 chat:message:* 键
        auto cursor = 0UL;
        std::vector<std::string> keys;
        do {
            cursor = redis_conn.scan(cursor, "chat:message:*", 100, std::back_inserter(keys));
            for (const auto& key : keys) {
                redis_conn.zpopmax(key, 200, std::back_inserter(messages));
            }
            keys.clear();
        } while (cursor != 0);
    } catch (const sw::redis::Error &err) {
        // 出错时返回空
        log_error("Failed to pop chat messages batch: {}", err.what());
        messages.clear();
    }
    return messages;
}

std::vector<std::string> RedisController::get_offline_messages(
    const std::string& user_ID,
    int64_t time,
    size_t count,
    const json& relation_data
) {
    // 私聊300，群聊200
    std::vector<std::string> offline_messages;
    try {
        if (relation_data.contains("friends") && relation_data["friends"].is_array() && !relation_data["friends"].empty()) {
            int cnt1 = 300 / relation_data["friends"].size();
            for (auto& friend_info : relation_data["friends"]) {
                std::string conv = std::min(user_ID, friend_info["id"].get<std::string>()) + "." +
                                   std::max(user_ID, friend_info["id"].get<std::string>());
                redis_conn.zrevrange("chat:messages:" + conv, 0, cnt1-1, std::back_inserter(offline_messages));
            }
        }
        if (relation_data.contains("groups") && relation_data["groups"].is_array() && !relation_data["groups"].empty()) {
            int cnt2 = 200 / relation_data["groups"].size();
            for (auto& group_info : relation_data["groups"]) {
                redis_conn.zrevrange("chat:messages:" + group_info["id"].get<std::string>(), 0, cnt2-1, std::back_inserter(offline_messages));
            }
        }
    } catch (const sw::redis::Error &err) {
        log_error("Failed to get offline messages: {}", err.what());
        offline_messages.clear();
    } catch (const std::exception& e) {
        log_error("Failed to get offline messages: {}", e.what());
        offline_messages.clear();
    }
    return offline_messages;
}

std::pair<bool, std::int64_t> RedisController::get_user_status(const std::string& user_ID) {
    auto key = "chat:user:" + user_ID + ":status";
    auto val = redis_conn.get(key);
    if (!val) {
        return {false, 0}; // 用户不存在或离线
    }
    auto jval = json::parse(*val);
    bool online = jval["online"];
    std::int64_t last_active = jval["last_active"];
    return {online, last_active};
}

void RedisController::set_user_status(const std::string& user_ID, bool online) {
    auto key = "chat:user:" + user_ID + ":status";
    json jval = {
        {"online", online},
        {"last_active", now_us()}
    };
    redis_conn.set(key, jval.dump());
}

void RedisController::del_user_status(const std::string& user_ID) {
    auto key = "chat:user:" + user_ID + ":status";
    redis_conn.del(key);
}

void RedisController::set_veri_code(const std::string& user_email, const std::string& veri_code) {
    auto key = "chat:email:" + user_email + ":veri_code";
    redis_conn.setex(key, std::chrono::seconds(300), veri_code);
}

std::string RedisController::get_veri_code(const std::string& user_email) {
    auto key = "chat:email:" + user_email + ":veri_code";
    auto val = redis_conn.get(key);
    if (!val) {
        return ""; // 用户不存在或验证码未设置
    }
    return *val;
}

void RedisController::del_veri_code(const std::string& user_email) {
    auto key = "chat:email:" + user_email + ":veri_code";
    redis_conn.del(key);
}

std::string RedisController::get_auth_code() {
    auto key = "chat:email:auth_code";
    auto val = redis_conn.get(key);
    if (!val) {
        return ""; // 如果没有设置过验证码
    }
    return *val;
}

std::string RedisController::get_email_addr() {
    auto key = "chat:email:server_addr";
    auto val = redis_conn.get(key);
    if (!val) {
        return ""; // 如果没有设置过邮箱地址
    }
    return *val;
}

/* ---------- 好友系统 ---------- */

bool RedisController::load_user_relations(const std::string& user_ID, json& relation_data, const json& blocked_by_data) {
    try {
        // 从传入的relation_data中提取好友和群组信息并缓存到Redis

        // 缓存好友信息 - 直接存储被屏蔽状态
        if (relation_data.contains("friends") && relation_data["friends"].is_array()) {
            std::string friends_key = "chat:user:" + user_ID + ":friends";
            redis_conn.del(friends_key); // 清空现有数据

            for (const auto& friend_info : relation_data["friends"]) {
                if (friend_info.contains("id")) {
                    std::string friend_id = friend_info["id"];

                    // 从blocked_by_data中获取被屏蔽状态
                    bool is_blocked_by_friend = false;
                    if (!blocked_by_data.empty() && blocked_by_data.contains(friend_id)) {
                        is_blocked_by_friend = blocked_by_data[friend_id].get<bool>();
                    }

                    redis_conn.hset(friends_key, friend_id, is_blocked_by_friend ? "1" : "0");
                }
            }
            redis_conn.expire(friends_key, 24 * 3600); // 24小时过期
        }

        // 缓存群组信息 - 只缓存用户的群组列表，群组详细信息只在未缓存时才加载
        if (relation_data.contains("groups") && relation_data["groups"].is_array()) {
            std::string groups_key = "chat:user:" + user_ID + ":groups";
            redis_conn.del(groups_key); // 清空现有数据

            for (const auto& group_info : relation_data["groups"]) {
                if (group_info.contains("id")) {
                    std::string group_id = group_info["id"];
                    redis_conn.sadd(groups_key, group_id);

                    // 只有当群组信息未缓存时才进行缓存，避免重复缓存
                    if (!is_group_cached(group_id)) {
                        log_debug("Group {} not cached, loading group data for first time", group_id);

                        if (group_info.contains("name") && group_info.contains("owner")) {
                            json group_cache_info;
                            group_cache_info["name"] = group_info["name"];
                            group_cache_info["owner"] = group_info["owner"];

                            // 缓存群组成员和管理员
                            if (group_info.contains("members") && group_info["members"].is_array()) {
                                std::vector<std::string> members;
                                std::vector<std::string> admins;

                                for (const auto& member_info : group_info["members"]) {
                                    if (member_info.contains("id")) {
                                        std::string member_id = member_info["id"];
                                        members.push_back(member_id);

                                        if (member_info.contains("is_admin") && member_info["is_admin"].get<bool>()) {
                                            admins.push_back(member_id);
                                        }
                                    }
                                }

                                // 更新成员计数
                                group_cache_info["member_count"] = static_cast<int>(members.size());

                                // 缓存群组基本信息、成员和管理员列表
                                cache_group_info(group_id, group_cache_info);
                                cache_group_members(group_id, members);
                                cache_group_admins(group_id, admins);

                                log_info("Cached group {} info with {} members, {} admins",
                                        group_id, members.size(), admins.size());
                            }
                        }
                    } else {
                        log_debug("Group {} already cached, skipping group data loading", group_id);
                    }
                }
            }
            redis_conn.expire(groups_key, 24 * 3600); // 24小时过期
        }

        log_info("Loaded user relations for {} into Redis cache", user_ID);
        return true;
    } catch (const std::exception& e) {
        log_error("Failed to load user relations for {}: {}", user_ID, e.what());
        return false;
    }
}

bool RedisController::unload_user(const std::string& user_ID) {
    try {
        auto key = "chat:user:" + user_ID + ":friends";
        redis_conn.del(key);
        key = "chat:user:" + user_ID + ":groups";
        redis_conn.del(key);
        key = "chat:user:" + user_ID + ":status";
        redis_conn.del(key);

        log_info("Unloaded user relations for {} from Redis cache", user_ID);
        return true;
    } catch (const std::exception& e) {
        log_error("Failed to unload user relations for {}: {}", user_ID, e.what());
        return false;
    }
}

bool RedisController::add_friend(const std::string& user_ID, const std::string& friend_ID, bool blocked) {
    try {
        std::string key = "chat:user:" + user_ID + ":friends";
        redis_conn.hset(key, friend_ID, blocked ? "1" : "0");
        log_debug("Added friend {} for user {} (blocked by friend: {})", friend_ID, user_ID, blocked);
        return true;
    } catch (const std::exception& e) {
        log_error("Failed to add friend {} for user {}: {}", friend_ID, user_ID, e.what());
        return false;
    }
}

bool RedisController::remove_friend(const std::string& user_ID, const std::string& friend_ID) {
    try {
        std::string key = "chat:user:" + user_ID + ":friends";
        redis_conn.hdel(key, friend_ID);
        log_debug("Removed friend {} for user {}", friend_ID, user_ID);
        return true;
    } catch (const std::exception& e) {
        log_error("Failed to remove friend {} for user {}: {}", friend_ID, user_ID, e.what());
        return false;
    }
}

bool RedisController::is_friend(const std::string& user_ID, const std::string& friend_ID) {
    try {
        std::string key = "chat:user:" + user_ID + ":friends";
        auto result = redis_conn.hexists(key, friend_ID);
        return result;
    } catch (const std::exception& e) {
        log_error("Failed to check if {} is friend of {}: {}", friend_ID, user_ID, e.what());
        return false;
    }
}

bool RedisController::is_blocked_by_friend(const std::string& user_ID, const std::string& friend_ID) {
    try {
        // 直接查询用户的好友缓存中的被屏蔽状态
        std::string key = "chat:user:" + user_ID + ":friends";
        auto result = redis_conn.hget(key, friend_ID);
        if (!result) {
            return false; // 缓存中没有该好友信息, 返回false
        }
        return *result == "1"; // "1"表示被屏蔽
    } catch (const std::exception& e) {
        log_error("Failed to check if {} is blocked by {}: {}", user_ID, friend_ID, e.what());
        return false;
    }
}

bool RedisController::set_blocked_by_friend(const std::string& user_ID, const std::string& friend_ID, bool blocked) {
    try {
        std::string key = "chat:user:" + user_ID + ":friends";
        redis_conn.hset(key, friend_ID, blocked ? "1" : "0");
        log_debug("Set blocked status for friend {} of user {} to {}", friend_ID, user_ID, blocked);
        return true;
    } catch (const std::exception& e) {
        log_error("Failed to set blocked status for friend {} of user {}: {}", friend_ID, user_ID, e.what());
        return false;
    }
}

std::vector<std::string> RedisController::get_friends_list(const std::string& user_ID) {
    try {
        std::string key = "chat:user:" + user_ID + ":friends";
        std::unordered_map<std::string, std::string> friends_map;
        redis_conn.hgetall(key, std::inserter(friends_map, friends_map.begin()));

        std::vector<std::string> friends;
        for (const auto& [friend_id, blocked_status] : friends_map) {
            friends.push_back(friend_id);
        }

        return friends;
    } catch (const std::exception& e) {
        log_error("Failed to get friends list for user {}: {}", user_ID, e.what());
        return {};
    }
}

std::vector<std::pair<std::string, bool>> RedisController::get_friends_with_block_status(const std::string& user_ID) {
    try {
        std::string key = "chat:user:" + user_ID + ":friends";
        std::unordered_map<std::string, std::string> friends_map;
        redis_conn.hgetall(key, std::inserter(friends_map, friends_map.begin()));

        std::vector<std::pair<std::string, bool>> friends;
        for (const auto& [friend_id, blocked_status] : friends_map) {
            bool is_blocked_by_friend = (blocked_status == "1");
            friends.emplace_back(friend_id, is_blocked_by_friend);
        }

        return friends;
    } catch (const std::exception& e) {
        log_error("Failed to get friends with block status for user {}: {}", user_ID, e.what());
        return {};
    }
}

/* ---------- 用户群组关系管理 ---------- */

std::vector<std::string> RedisController::get_user_groups(const std::string& user_ID) {
    try {
        std::string key = "chat:user:" + user_ID + ":groups";
        std::unordered_set<std::string> groups_set;
        redis_conn.smembers(key, std::inserter(groups_set, groups_set.begin()));

        std::vector<std::string> groups(groups_set.begin(), groups_set.end());
        return groups;
    } catch (const std::exception& e) {
        log_error("Failed to get groups for user {}: {}", user_ID, e.what());
        return {};
    }
}

bool RedisController::add_user_to_group(const std::string& user_ID, const std::string& group_ID) {
    try {
        std::string key = "chat:user:" + user_ID + ":groups";
        redis_conn.sadd(key, group_ID);
        redis_conn.expire(key, 24 * 3600); // 24小时过期
        log_debug("Added group {} to user {}'s groups", group_ID, user_ID);
        return true;
    } catch (const std::exception& e) {
        log_error("Failed to add group {} to user {}: {}", group_ID, user_ID, e.what());
        return false;
    }
}

bool RedisController::remove_user_from_group(const std::string& user_ID, const std::string& group_ID) {
    try {
        std::string key = "chat:user:" + user_ID + ":groups";
        redis_conn.srem(key, group_ID);
        log_debug("Removed group {} from user {}'s groups", group_ID, user_ID);
        return true;
    } catch (const std::exception& e) {
        log_error("Failed to remove group {} from user {}: {}", group_ID, user_ID, e.what());
        return false;
    }
}

/* ---------- 群组系统 ---------- */

bool RedisController::cache_group_members(const std::string& group_ID, const std::vector<std::string>& members) {
    try {
        std::string key = "chat:group:" + group_ID + ":members";
        redis_conn.del(key); // 清空现有成员

        if (!members.empty()) {
            for (const auto& member : members) {
                redis_conn.sadd(key, member);
            }
        }

        // 群组信息持久化, 不设置过期时间
        log_debug("Cached {} members for group {}", members.size(), group_ID);
        return true;
    } catch (const std::exception& e) {
        log_error("Failed to cache members for group {}: {}", group_ID, e.what());
        return false;
    }
}

bool RedisController::cache_group_admins(const std::string& group_ID, const std::vector<std::string>& admins) {
    try {
        std::string key = "chat:group:" + group_ID + ":admins";
        redis_conn.del(key); // 清空现有管理员

        if (!admins.empty()) {
            for (const auto& admin : admins) {
                redis_conn.sadd(key, admin);
            }
        }

        // 群组信息持久化, 不设置过期时间
        log_debug("Cached {} admins for group {}", admins.size(), group_ID);
        return true;
    } catch (const std::exception& e) {
        log_error("Failed to cache admins for group {}: {}", group_ID, e.what());
        return false;
    }
}

bool RedisController::cache_group_info(const std::string& group_ID, const json& group_info) {
    try {
        std::string key = "chat:group:" + group_ID + ":info";

        // 存储群组基本信息
        if (group_info.contains("name")) {
            redis_conn.hset(key, "name", group_info["name"].get<std::string>());
        }
        if (group_info.contains("owner")) {
            redis_conn.hset(key, "owner", group_info["owner"].get<std::string>());
        }
        if (group_info.contains("member_count")) {
            redis_conn.hset(key, "member_count", std::to_string(group_info["member_count"].get<int>()));
        }

        // 群组信息持久化, 不设置过期时间
        log_debug("Cached info for group {}", group_ID);
        return true;
    } catch (const std::exception& e) {
        log_error("Failed to cache info for group {}: {}", group_ID, e.what());
        return false;
    }
}

bool RedisController::add_group_member(const std::string& group_ID, const std::string& user_ID) {
    try {
        std::string members_key = "chat:group:" + group_ID + ":members";
        std::string info_key = "chat:group:" + group_ID + ":info";

        // 添加到成员集合
        redis_conn.sadd(members_key, user_ID);

        // 更新成员数量
        redis_conn.hincrby(info_key, "member_count", 1);

        // 添加到用户的群组列表
        std::string user_groups_key = "chat:user:" + user_ID + ":groups";
        redis_conn.sadd(user_groups_key, group_ID);

        log_debug("Added user {} to group {}", user_ID, group_ID);
        return true;
    } catch (const std::exception& e) {
        log_error("Failed to add user {} to group {}: {}", user_ID, group_ID, e.what());
        return false;
    }
}

bool RedisController::remove_group_member(const std::string& group_ID, const std::string& user_ID) {
    try {
        std::string members_key = "chat:group:" + group_ID + ":members";
        std::string admins_key = "chat:group:" + group_ID + ":admins";
        std::string info_key = "chat:group:" + group_ID + ":info";

        // 从成员集合中移除
        redis_conn.srem(members_key, user_ID);

        // 从管理员集合中移除（如果是管理员）
        redis_conn.srem(admins_key, user_ID);

        // 更新成员数量
        redis_conn.hincrby(info_key, "member_count", -1);

        // 从用户的群组列表中移除
        std::string user_groups_key = "chat:user:" + user_ID + ":groups";
        redis_conn.srem(user_groups_key, group_ID);

        log_debug("Removed user {} from group {}", user_ID, group_ID);
        return true;
    } catch (const std::exception& e) {
        log_error("Failed to remove user {} from group {}: {}", user_ID, group_ID, e.what());
        return false;
    }
}

bool RedisController::add_group_admin(const std::string& group_ID, const std::string& user_ID) {
    try {
        std::string key = "chat:group:" + group_ID + ":admins";
        redis_conn.sadd(key, user_ID);
        log_debug("Added admin {} to group {}", user_ID, group_ID);
        return true;
    } catch (const std::exception& e) {
        log_error("Failed to add admin {} to group {}: {}", user_ID, group_ID, e.what());
        return false;
    }
}

bool RedisController::remove_group_admin(const std::string& group_ID, const std::string& user_ID) {
    try {
        std::string key = "chat:group:" + group_ID + ":admins";
        redis_conn.srem(key, user_ID);
        log_debug("Removed admin {} from group {}", user_ID, group_ID);
        return true;
    } catch (const std::exception& e) {
        log_error("Failed to remove admin {} from group {}: {}", user_ID, group_ID, e.what());
        return false;
    }
}

bool RedisController::remove_group(const std::string& group_ID) {
    try {
        std::vector<std::string> keys_to_delete = {
            "chat:group:" + group_ID + ":members",
            "chat:group:" + group_ID + ":admins",
            "chat:group:" + group_ID + ":info"
        };

        for (const auto& key : keys_to_delete) {
            redis_conn.del(key);
        }

        log_info("Removed group {} from cache", group_ID);
        return true;
    } catch (const std::exception& e) {
        log_error("Failed to remove group {}: {}", group_ID, e.what());
        return false;
    }
}

bool RedisController::is_group_member(const std::string& group_ID, const std::string& user_ID) {
    try {
        std::string key = "chat:group:" + group_ID + ":members";
        return redis_conn.sismember(key, user_ID);
    } catch (const std::exception& e) {
        log_error("Failed to check if user {} is member of group {}: {}", user_ID, group_ID, e.what());
        return false;
    }
}

bool RedisController::is_group_admin(const std::string& group_ID, const std::string& user_ID) {
    try {
        std::string key = "chat:group:" + group_ID + ":admins";
        return redis_conn.sismember(key, user_ID);
    } catch (const std::exception& e) {
        log_error("Failed to check if user {} is admin of group {}: {}", user_ID, group_ID, e.what());
        return false;
    }
}

std::vector<std::string> RedisController::get_group_members(const std::string& group_ID) {
    try {
        std::string key = "chat:group:" + group_ID + ":members";
        std::unordered_set<std::string> members_set;
        redis_conn.smembers(key, std::inserter(members_set, members_set.begin()));

        std::vector<std::string> members(members_set.begin(), members_set.end());
        return members;
    } catch (const std::exception& e) {
        log_error("Failed to get members of group {}: {}", group_ID, e.what());
        return {};
    }
}

std::vector<std::string> RedisController::get_group_admins(const std::string& group_ID) {
    try {
        std::string key = "chat:group:" + group_ID + ":admins";
        std::unordered_set<std::string> admins_set;
        redis_conn.smembers(key, std::inserter(admins_set, admins_set.begin()));

        std::vector<std::string> admins(admins_set.begin(), admins_set.end());
        return admins;
    } catch (const std::exception& e) {
        log_error("Failed to get admins of group {}: {}", group_ID, e.what());
        return {};
    }
}

json RedisController::get_group_info(const std::string& group_ID) {
    try {
        std::string key = "chat:group:" + group_ID + ":info";
        std::unordered_map<std::string, std::string> info_map;
        redis_conn.hgetall(key, std::inserter(info_map, info_map.begin()));

        json group_info;
        for (const auto& [field, value] : info_map) {
            if (field == "member_count") {
                group_info[field] = std::stoi(value);
            } else {
                group_info[field] = value;
            }
        }

        return group_info;
    } catch (const std::exception& e) {
        log_error("Failed to get info for group {}: {}", group_ID, e.what());
        return json{};
    }
}

/* ---------- 群组缓存检查 ---------- */

bool RedisController::is_group_cached(const std::string& group_ID) {
    return are_group_members_cached(group_ID) &&
           are_group_admins_cached(group_ID) &&
           is_group_info_cached(group_ID);
}

bool RedisController::are_group_members_cached(const std::string& group_ID) {
    try {
        std::string key = "chat:group:" + group_ID + ":members";
        return redis_conn.exists(key);
    } catch (const std::exception& e) {
        log_error("Failed to check if group {} members are cached: {}", group_ID, e.what());
        return false;
    }
}

bool RedisController::are_group_admins_cached(const std::string& group_ID) {
    try {
        std::string key = "chat:group:" + group_ID + ":admins";
        return redis_conn.exists(key);
    } catch (const std::exception& e) {
        log_error("Failed to check if group {} admins are cached: {}", group_ID, e.what());
        return false;
    }
}

bool RedisController::is_group_info_cached(const std::string& group_ID) {
    try {
        std::string key = "chat:group:" + group_ID + ":info";
        return redis_conn.exists(key);
    } catch (const std::exception& e) {
        log_error("Failed to check if group {} info is cached: {}", group_ID, e.what());
        return false;
    }
}

/* ==================== 文件存储 ==================== */

std::string RedisController::get_file_storage_path() {
    std::string key = "chat:file:storage";
    return redis_conn.get(key).value();
}
