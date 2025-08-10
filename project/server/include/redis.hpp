#pragma once

#include <sw/redis++/redis++.h>
#include <string>
#include <utility>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

/*
用户在线状态：
chat:user:<user_id>:status -> JSON字符串 {
    "online" : true/false,
    "last_active" : <timestamp> // Unix时间戳, 用于心跳检测
}

用户验证码（邮箱验证）：
chat:email:<user_email>:veri_code -> "123456" // 6位数字验证码
过期时间：300秒（5分钟）

用户关系缓存：
chat:user:<user_id>:friends -> Hash {
    "friend_id1" : "0/1",  // 0=未被对方屏蔽, 1=被对方屏蔽
    "friend_id2" : "0/1",
    ...
}
chat:user:<user_id>:groups -> Set {group_id1, group_id2, ...} // 群组ID列表

群组成员缓存：
chat:group:<group_id>:members -> Set {user_id1, user_id2, user_id3, ...}

群组管理员缓存：
chat:group:<group_id>:admins -> Set {admin_id1, admin_id2, ...}

群组信息缓存：
chat:group:<group_id>:info -> Hash {
    "name" : "群组名称",
    "owner" : "群主ID",
    "member_count" : "成员数量"
}

消息批量缓存（用于异步落盘 MySQL）：
Key: chat:messages
Type: List
Value: envelope(chatmessage)的string
*/


class RedisController {
public:
    using Redis = sw::redis::Redis;

    RedisController();

/* ==================== 消息批量缓存 ==================== */

    // 将序列化后的消息加入缓存队列
    bool cache_chat_message(const std::string& serialized_msg);

    // 批量取出消息（最多 count 条），并从 Redis 删除
    // 返回的 vector 中每个元素是 protobuf ChatMessage 的二进制
    std::vector<std::string> pop_chat_messages_batch(size_t count);

    // 获取当前缓存队列的长度
    size_t get_chat_message_queue_size();

/* ==================== 用户状态 ==================== */

    std::pair<bool, std::time_t> get_user_status(const std::string& user_ID);

    void set_user_status(const std::string& user_ID, bool online);

/* ==================== 验证码 ==================== */

    void set_veri_code(const std::string& user_email, const std::string& veri_code);

    std::string get_veri_code(const std::string& user_email);

    void del_veri_code(const std::string& user_email);

    std::string get_auth_code();
    std::string get_email_addr();

/* ==================== 用户关系网 ==================== */

    // 加载用户关系数据到Redis缓存
    // relation_data: 来自get_relation_net的好友和群组数据（不使用其中的blocked字段）
    // blocked_by_data: 专门的被屏蔽状态数据, 存储到friends缓存中
    bool load_user_relations(const std::string& user_ID, json& relation_data, const json& blocked_by_data = json{});

    bool unload_user_relations(const std::string& user_ID);

    bool add_friend(const std::string& user_ID, const std::string& friend_ID, bool blocked);

    bool remove_friend(const std::string& user_ID, const std::string& friend_ID);

    bool is_friend(const std::string& user_ID, const std::string& friend_ID);

    // 用法相反
    bool is_blocked_by_friend(const std::string& user_ID, const std::string& friend_ID);

    bool set_blocked_by_friend(const std::string& user_ID, const std::string& friend_ID, bool blocked);

    std::vector<std::string> get_friends_list(const std::string& user_ID);

    std::vector<std::pair<std::string, bool>> get_friends_with_block_status(const std::string& user_ID);

    std::vector<std::string> get_user_groups(const std::string& user_ID);

    bool add_user_to_group(const std::string& user_ID, const std::string& group_ID);

    bool remove_user_from_group(const std::string& user_ID, const std::string& group_ID);

/* ==================== 群组 ==================== */

    bool cache_group_members(const std::string& group_ID, const std::vector<std::string>& members);
    bool cache_group_admins(const std::string& group_ID, const std::vector<std::string>& admins);
    bool cache_group_info(const std::string& group_ID, const json& group_info);

    bool add_group_member(const std::string& group_ID, const std::string& user_ID);
    bool remove_group_member(const std::string& group_ID, const std::string& user_ID);

    bool add_group_admin(const std::string& group_ID, const std::string& user_ID);
    bool remove_group_admin(const std::string& group_ID, const std::string& user_ID);

    bool remove_group(const std::string& group_ID);

    bool is_group_member(const std::string& group_ID, const std::string& user_ID);
    std::vector<std::string> get_group_members(const std::string& group_ID);

    bool is_group_admin(const std::string& group_ID, const std::string& user_ID);
    std::vector<std::string> get_group_admins(const std::string& group_ID);

    json get_group_info(const std::string& group_ID);

/* ==================== 缓存状态检查 ==================== */

    bool is_group_cached(const std::string& group_ID);

    bool are_group_members_cached(const std::string& group_ID);

    bool are_group_admins_cached(const std::string& group_ID);

    bool is_group_info_cached(const std::string& group_ID);

/* ==================== 文件 ==================== */

    std::string get_file_storage_path();

private:
    Redis redis_conn; // 实际连接对象
};
