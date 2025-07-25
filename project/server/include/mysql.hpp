#pragma once
#include <mysql/mysql.h>
#include <string>
#include <vector>
#include <mutex>

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
    bool update_user_status(const std::string& user_ID, bool online);

/* ---------- 好友 ---------- */

    bool add_friend(const std::string& user_ID, const std::string& friend_ID);
    bool delete_friend(const std::string& user_ID, const std::string& friend_ID);
    bool block_friend(const std::string& user_ID, const std::string& friend_ID);
    bool unblock_friend(const std::string& user_ID, const std::string& friend_ID);
    bool is_friend(const std::string& user_ID, const std::string& friend_ID);
    bool search_user(const std::string& searched_ID);

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


/* ---------- 文件 ---------- */
};