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

    // 用户操作
    bool register_user(const std::string& username, const std::string& password_hash);
    bool check_user(const std::string& username, const std::string& password_hash);

    // 聊天记录
    bool insert_message(const std::string& sender, const std::string& receiver,
                        const std::string& content, int64_t timestamp);

    std::vector<std::tuple<std::string, std::string, std::string, int64_t>>
    get_messages(const std::string& user1, const std::string& user2, size_t limit = 50);

    // 文件记录
    bool insert_file(const std::string& hash, const std::string& filename, size_t size);
    bool file_exists(const std::string& hash);

/* ---------- 用户系统---------- */
    bool do_email_exist(const std::string& email);
    bool do_user_id_exist(const std::string& user_ID);
    bool insert_user(
        const std::string& user_ID,
        const std::string& email,
        const std::string& user_password);
};