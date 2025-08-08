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
*/

class RedisController {
public:
    using Redis = sw::redis::Redis;

    RedisController();

/* ==================== 用户状态管理 ==================== */

    // 获取用户在线状态和最后活跃时间
    std::pair<bool, std::time_t> get_user_status(const std::string& user_ID);

    // 设置用户在线状态（自动更新最后活跃时间）
    void set_user_status(const std::string& user_ID, bool online);

/* ==================== 验证码管理 ==================== */

    // 设置邮箱验证码（5分钟过期）
    void set_veri_code(const std::string& user_email, const std::string& veri_code);

    // 获取邮箱验证码
    std::string get_veri_code(const std::string& user_email);

    // 删除邮箱验证码
    void del_veri_code(const std::string& user_email);

    std::string get_auth_code();
    std::string get_email_addr();

/* ==================== 用户关系网管理 ==================== */

    // 加载用户关系数据到Redis缓存
    // relation_data: 来自get_relation_net的好友和群组数据（不使用其中的blocked字段）
    // blocked_by_data: 专门的被屏蔽状态数据, 存储到friends缓存中
    bool load_user_relations(const std::string& user_ID, json& relation_data, const json& blocked_by_data = json{});

    // 从Redis缓存中卸载用户关系数据
    bool unload_user_relations(const std::string& user_ID, const json& relation_data);

    // 添加好友关系（blocked表示是否被该好友屏蔽）
    bool add_friend(const std::string& user_ID, const std::string& friend_ID, bool blocked);

    // 移除好友关系
    bool remove_friend(const std::string& user_ID, const std::string& friend_ID);

    // 检查是否为好友关系
    bool is_friend(const std::string& user_ID, const std::string& friend_ID);

    // 检查用户是否被好友屏蔽（从friends缓存中查询）
    bool is_blocked_by_friend(const std::string& user_ID, const std::string& friend_ID);

    // 更新被好友屏蔽状态
    bool set_blocked_by_friend(const std::string& user_ID, const std::string& friend_ID, bool blocked);

    // 获取好友ID列表
    std::vector<std::string> get_friends_list(const std::string& user_ID);

    // 获取好友列表及其被屏蔽状态
    std::vector<std::pair<std::string, bool>> get_friends_with_block_status(const std::string& user_ID);

    // 获取用户所属的群组列表
    std::vector<std::string> get_user_groups(const std::string& user_ID);

    // 添加用户到群组关系（用户侧）
    bool add_user_to_group(const std::string& user_ID, const std::string& group_ID);

    // 从用户群组关系中移除（用户侧）
    bool remove_user_from_group(const std::string& user_ID, const std::string& group_ID);

/* ==================== 群组管理 ==================== */

    // 群组缓存操作
    bool cache_group_members(const std::string& group_ID, const std::vector<std::string>& members);
    bool cache_group_admins(const std::string& group_ID, const std::vector<std::string>& admins);
    bool cache_group_info(const std::string& group_ID, const json& group_info);

    // 群组成员管理
    bool add_group_member(const std::string& group_ID, const std::string& user_ID);
    bool remove_group_member(const std::string& group_ID, const std::string& user_ID);

    // 群组管理员管理
    bool add_group_admin(const std::string& group_ID, const std::string& user_ID);
    bool remove_group_admin(const std::string& group_ID, const std::string& user_ID);

    // 群组操作
    bool remove_group(const std::string& group_ID);

/* ==================== 群组查询 ==================== */

    // 群组成员查询
    bool is_group_member(const std::string& group_ID, const std::string& user_ID);
    std::vector<std::string> get_group_members(const std::string& group_ID);

    // 群组管理员查询
    bool is_group_admin(const std::string& group_ID, const std::string& user_ID);
    std::vector<std::string> get_group_admins(const std::string& group_ID);

    // 群组信息查询
    json get_group_info(const std::string& group_ID);

/* ==================== 缓存状态检查 ==================== */

    // 检查群组是否完全缓存（成员、管理员、信息都存在）
    bool is_group_cached(const std::string& group_ID);

    // 检查群组成员是否已缓存
    bool are_group_members_cached(const std::string& group_ID);

    // 检查群组管理员是否已缓存
    bool are_group_admins_cached(const std::string& group_ID);

    // 检查群组信息是否已缓存
    bool is_group_info_cached(const std::string& group_ID);

/* ==================== 文件存储 ==================== */

    std::string get_file_storage_path();

/* ==================== 在线消息缓存 ==================== */
//         (待实现：缓存最近2000条消息)

private:
    Redis redis_conn; // 实际连接对象
};
