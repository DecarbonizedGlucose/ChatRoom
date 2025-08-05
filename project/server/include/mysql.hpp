#pragma once
#include <mysql/mysql.h>
#include <string>
#include <vector>
#include <mutex>
#include <optional>
#include "../../global/abstract/datatypes.hpp"

class MySQLController {
private:
    MYSQL* conn;
    std::mutex db_mutex;

    std::string host, user, password, dbname;
    unsigned int port;

    // 邮箱标准化处理
    std::string normalize_email(const std::string& email) const;

public:
    MySQLController(const std::string& host,
             const std::string& user,
             const std::string& password,
             const std::string& dbname,
             unsigned int port = 3306);

    ~MySQLController();

    bool connect();
    void disconnect();
    bool is_connected() const;

    // 通用执行
    bool execute(const std::string& query);
    std::vector<std::vector<std::string>> query(const std::string& sql);

/* ---------- 用户系统---------- */

    bool do_email_exist(const std::string& email);
    bool do_user_id_exist(const std::string& user_ID);
    bool insert_user(
        const std::string& user_ID,
        const std::string& email,
        const std::string& password_hash);
    bool check_user_pswd(const std::string& email, const std::string& password_hash);
    void update_user_last_active(const std::string& email);
    std::string get_user_id_from_email(const std::string& email);
    std::string get_user_email_from_id(const std::string& user_ID);
    bool update_user_status(const std::string& user_ID, bool online);

/* ---------- 好友 ---------- */

    bool add_friend(const std::string& user_ID, const std::string& friend_ID);
    bool delete_friend(const std::string& user_ID, const std::string& friend_ID);
    bool block_friend(const std::string& user_ID, const std::string& friend_ID);
    bool unblock_friend(const std::string& user_ID, const std::string& friend_ID);
    bool is_friend(const std::string& user_ID, const std::string& friend_ID);
    bool is_blocked_by_friend(const std::string& user_ID, const std::string& friend_ID);

    // 查询函数
    std::vector<std::string> get_friends_list(const std::string& user_ID);
    std::vector<std::pair<std::string, bool>> get_friends_with_block_status(const std::string& user_ID);

/* ---------- 群组 ---------- */

    bool create_group(const std::string& group_ID,
                      const std::string& group_name,
                      const std::string& owner_ID);
    bool add_user_to_group(const std::string& group_ID,
                           const std::string& user_ID);
    bool kick_user_from_group(const std::string& group_ID,
                                const std::string& user_ID);
    bool remove_user_from_group(const std::string& group_ID,
                                const std::string& user_ID);
    bool disband_group(const std::string& group_ID);
    bool search_group(const std::string& group_ID);
    bool is_user_in_group(const std::string& group_ID, const std::string& user_ID);
    bool add_group_admin(const std::string& group_ID,
                         const std::string& user_ID);
    bool remove_group_admin(const std::string& group_ID,
                            const std::string& user_ID);

    // 查询函数
    std::vector<std::string> get_user_groups(const std::string& user_ID);
    std::vector<std::pair<std::string, bool>> get_group_members_with_admin_status(const std::string& group_ID);
    std::string get_group_owner(const std::string& group_ID);
    std::string get_group_name(const std::string& group_ID);

/* ---------- 聊天记录 ---------- */

    bool add_chat_message(
        const std::string& sender_ID,
        const std::string& receiver_ID,
        bool is_to_group,
        std::time_t timestamp,
        const std::string& text_content,
        bool pin = false,
        const std::string& file_name = "",
        const std::size_t file_size = 0,
        const std::string& file_hash = "");

    // 获取用户的离线消息（从last_active时间点之后的消息）
    std::vector<std::tuple<std::string, std::string, bool, std::time_t, std::string, bool, std::string, std::size_t, std::string>>
    get_offline_messages(const std::string& user_ID, std::time_t last_active_time, int limit = 200);

    // 获取用户的last_active时间
    std::time_t get_user_last_active(const std::string& user_ID);


/* ---------- 文件 ---------- */

    // 查询目前文件数量
    int get_file_count();

    // 查询给出的file_hash是否在数据库中存在
    bool file_hash_exists(const std::string& file_hash);

    // 通过file_hash给出file_id
    std::string get_file_id_by_hash(const std::string& file_hash);

    // 通过file_id给出file_hash
    std::string get_file_hash_by_id(const std::string& file_id);

    // 通过file_id获取文件信息(文件名和大小)
    // 返回值: optional<pair<file_name, file_size>>
    std::optional<std::pair<std::string, size_t>> get_file_info(const std::string& file_id);

    // 仅生成file_id（不插入数据库）
    std::string generate_file_id_only(const std::string& file_hash);

    // 通过file_hash生成新的file_id
    // 如果file_hash存在，返回"", 反之，返回 "File_" + std::tostring(num), num是已存在的文件数量
    std::string generate_file_id_by_hash(const std::string& file_hash, std::size_t file_size);

/* ---------- 通知/请求 ---------- */

    int store_command(const CommandRequest& cmd, bool managed = false);  // 返回命令ID
    bool delete_command(int command_id);
    bool update_command_status(int command_id, bool managed);
    int get_command_status(int command_id);
    CommandRequest get_command(int command_id);

    bool add_pending_command(const std::string& user_id, int command_id);
    bool remove_pending_command(const std::string& user_id, int command_id);
    std::vector<CommandRequest> get_pending_commands(const std::string& user_id);
    bool clear_pending_commands(const std::string& user_id);

    bool add_command_to_all_admin(
        int command_id,
        const std::string& group_ID);
    bool add_command_to_all_admin_except(
        const std::string& user_ID,
        int command_id,
        const std::string& group_ID);
    bool remove_command_from_all_admin(
        int command_id,
        const std::string& group_ID);
};