#include "../include/mysql.hpp"
#include <iostream>

MySQLController::MySQLController(const std::string& host,
                                 const std::string& user,
                                 const std::string& password,
                                 const std::string& dbname,
                                 unsigned int port)
    : conn(nullptr), host(host), user(user), password(password), dbname(dbname), port(port) {}

MySQLController::~MySQLController() {
    disconnect();
}

bool MySQLController::connect() {
    conn = mysql_init(nullptr);
    if (!conn) return false;

    if (!mysql_real_connect(conn, host.c_str(), user.c_str(), password.c_str(),
                            dbname.c_str(), port, nullptr, 0)) {
        std::cerr << "MySQL connection failed: " << mysql_error(conn) << std::endl;
        return false;
    }
    return true;
}

void MySQLController::disconnect() {
    if (conn) {
        mysql_close(conn);
        conn = nullptr;
    }
}

bool MySQLController::is_connected() const {
    return conn != nullptr;
}

bool MySQLController::execute(const std::string& query) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!conn) return false;
    return mysql_query(conn, query.c_str()) == 0;
}

std::vector<std::vector<std::string>> MySQLController::query(const std::string& sql) {
    std::lock_guard<std::mutex> lock(db_mutex);
    std::vector<std::vector<std::string>> result;
    if (!conn) return result;

    if (mysql_query(conn, sql.c_str()) != 0) return result;
    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) return result;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        std::vector<std::string> row_data;
        for (unsigned int i = 0; i < mysql_num_fields(res); ++i) {
            row_data.emplace_back(row[i] ? row[i] : "");
        }
        result.emplace_back(std::move(row_data));
    }
    mysql_free_result(res);
    return result;
}

bool MySQLController::register_user(const std::string& username, const std::string& password_hash) {
    std::string sql = "INSERT INTO users (username, password) VALUES ('" + username + "', '" + password_hash + "');";
    return execute(sql);
}

bool MySQLController::check_user(const std::string& username, const std::string& password_hash) {
    std::string sql = "SELECT * FROM users WHERE username='" + username + "' AND password='" + password_hash + "';";
    auto rows = query(sql);
    return !rows.empty();
}

bool MySQLController::insert_message(const std::string& sender, const std::string& receiver,
                                     const std::string& content, int64_t timestamp) {
    std::string sql = "INSERT INTO messages (sender, receiver, content, timestamp) VALUES ('" +
                      sender + "', '" + receiver + "', '" + content + "', " + std::to_string(timestamp) + ");";
    return execute(sql);
}

std::vector<std::tuple<std::string, std::string, std::string, int64_t>>
MySQLController::get_messages(const std::string& user1, const std::string& user2, size_t limit) {
    std::vector<std::tuple<std::string, std::string, std::string, int64_t>> messages;
    std::string sql = "SELECT sender, receiver, content, timestamp FROM messages WHERE ";
    sql += "(sender='" + user1 + "' AND receiver='" + user2 + "') OR (sender='" + user2 + "' AND receiver='" + user1 + "') ";
    sql += "ORDER BY timestamp DESC LIMIT " + std::to_string(limit) + ";";

    auto rows = query(sql);
    for (const auto& row : rows) {
        if (row.size() >= 4) {
            messages.emplace_back(row[0], row[1], row[2], std::stoll(row[3]));
        }
    }
    return messages;
}

bool MySQLController::insert_file(const std::string& hash, const std::string& filename, size_t size) {
    std::string sql = "INSERT INTO files (hash, filename, size) VALUES ('" +
                      hash + "', '" + filename + "', " + std::to_string(size) + ");";
    return execute(sql);
}

bool MySQLController::file_exists(const std::string& hash) {
    std::string sql = "SELECT hash FROM files WHERE hash='" + hash + "';";
    auto rows = query(sql);
    return !rows.empty();
}

/* ---------- 用户系统---------- */

bool MySQLController::do_email_exist(const std::string& email) {
    std::string query = "SELECT COUNT(*) FROM users WHERE user_email = '" + email + "';";
    if (mysql_query(conn, query.c_str())) {
        std::cerr << "Email check failed: " << mysql_error(conn) << std::endl;
        return false; // 查询失败
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) return false;

    MYSQL_ROW row = mysql_fetch_row(res);
    bool exists = (row && std::stoi(row[0]) > 0);
    mysql_free_result(res);
    return exists;
}

bool MySQLController::do_user_id_exist(const std::string& user_ID) {
    std::string query = "SELECT COUNT(*) FROM users WHERE user_id = '" + user_ID + "';";
    if (mysql_query(conn, query.c_str())) {
        std::cerr << "User ID check failed: " << mysql_error(conn) << std::endl;
        return false; // 查询失败
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) return false;

    MYSQL_ROW row = mysql_fetch_row(res);
    bool exists = (row && std::stoi(row[0]) > 0);
    mysql_free_result(res);
    return exists;
}

bool MySQLController::insert_user(
    const std::string& user_ID,
    const std::string& email,
    const std::string& user_password) {
    std::string sql = "INSERT INTO users (user_id, user_email, user_password) VALUES ('" +
                      user_ID + "', '" + email + "', '" + user_password + "');";
    return execute(sql);
}