#pragma once
#include <SQLiteCpp/SQLiteCpp.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <ctime>
#include <functional>

class SQLiteController {
private:
    std::unique_ptr<SQLite::Database> db;  // SQLite数据库连接对象
    std::mutex db_mutex;                   // 数据库操作互斥锁, 确保线程安全
    std::string db_path;                   // 数据库文件路径

    /* ---------- 通用辅助模板方法 ---------- */

    template<typename... Args>
    bool execute_stmt(const std::string& sql, const std::string& operation, Args&&... args);

    template<typename T>
    std::vector<T> query_list(const std::string& sql, const std::string& operation,
                             std::function<T(SQLite::Statement&)> mapper);

    template<typename T, typename... Args>
    std::vector<T> query_list_with_params(const std::string& sql, const std::string& operation,
                                         std::function<T(SQLite::Statement&)> mapper, Args&&... args);

    template<typename T, typename... Args>
    T query_single(const std::string& sql, const std::string& operation, T default_value,
                  std::function<T(SQLite::Statement&)> mapper, Args&&... args);

    void bind_params(SQLite::Statement& stmt, int index);

    template<typename T, typename... Args>
    void bind_params(SQLite::Statement& stmt, int index, T&& first, Args&&... rest);

public:
    SQLiteController(const std::string& database_path);
    ~SQLiteController();
    bool connect();
    void disconnect();
    bool is_connected() const;

    /* ---------- 通用数据库操作 ---------- */

    bool execute(const std::string& query);

    std::vector<std::vector<std::string>> query(const std::string& sql);

/* ---------- 用户系统 ---------- */

    bool store_user_info(
        const std::string& user_ID,
        const std::string& email,
        const std::string& password_hash);

    bool get_stored_user_info(std::string& user_ID, std::string& email);

    bool clear_user_info();

    bool clear_all_user_data();

/* ---------- 好友 ---------- */

    bool cache_friend(const std::string& user_ID, const std::string& friend_ID, bool is_blocked = false);

    bool remove_friend_cache(const std::string& user_ID, const std::string& friend_ID);

    bool update_friend_block_status(const std::string& user_ID, const std::string& friend_ID, bool is_blocked);

    std::vector<std::string> get_cached_friends_list(const std::string& user_ID);

    std::vector<std::pair<std::string, bool>> get_cached_friends_with_block_status(const std::string& user_ID);

/* ---------- 群 ---------- */

    bool cache_group(const std::string& group_ID, const std::string& group_name, const std::string& owner_ID);

    bool cache_group_member(const std::string& group_ID, const std::string& user_ID, bool is_admin = false);

    bool remove_group_cache(const std::string& group_ID);

    bool remove_group_member_cache(const std::string& group_ID, const std::string& user_ID);

    bool update_group_admin_status(const std::string& group_ID, const std::string& user_ID, bool is_admin);

    std::vector<std::string> get_cached_user_groups(const std::string& user_ID);

    std::vector<std::pair<std::string, bool>> get_cached_group_members_with_admin_status(const std::string& group_ID);

    std::string get_cached_group_owner(const std::string& group_ID);

    std::string get_cached_group_name(const std::string& group_ID);

    bool is_group_member(const std::string& group_ID, const std::string& user_ID);

    bool is_group_admin(const std::string& group_ID, const std::string& user_ID);

/* ---------- 聊天记录 ---------- */

    bool cache_chat_message(
        const std::string& sender_ID,
        const std::string& receiver_ID,
        bool is_group,
        std::int64_t timestamp,
        const std::string& text_content,
        bool pin = false,
        const std::string& file_name = "",
        std::size_t file_size = 0,
        const std::string& file_hash = "");

    std::vector<std::vector<std::string>> get_private_chat_history(
        const std::string& user_A,
        const std::string& user_B,
        int limit = 100);

    std::vector<std::vector<std::string>> get_group_chat_history(
        const std::string& group_ID,
        int limit = 100);

/* ---------- 自毁 ---------- */

    bool delete_user_data(const std::string& user_ID);
};

