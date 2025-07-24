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

/* ---------- 好友 & 群组 ---------- */

/* ---------- 聊天记录 ---------- */

/* ---------- 文件 ---------- */
};