#include "../include/mysql.hpp"
#include "../../global/include/logging.hpp"
#include <iostream>
#include <algorithm>
#include <cctype>

MySQLController::MySQLController(const std::string& host,
                                 const std::string& user,
                                 const std::string& password,
                                 const std::string& dbname,
                                 unsigned int port)
    : conn(nullptr), host(host), user(user),
    password(password), dbname(dbname), port(port) {}

MySQLController::~MySQLController() {
    disconnect();
}

bool MySQLController::connect() {
    conn = mysql_init(nullptr);
    if (!conn) return false;

    if (!mysql_real_connect(conn, host.c_str(), user.c_str(), password.c_str(),
                            dbname.c_str(), port, nullptr, 0)) {
        log_error("MySQL connection failed: " + std::string(mysql_error(conn)));
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

std::string MySQLController::normalize_email(const std::string& email) const {
    std::string normalized = email;
    // 转换为小写
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    // 去除前后空格
    normalized.erase(0, normalized.find_first_not_of(" \t\n\r\f\v"));
    normalized.erase(normalized.find_last_not_of(" \t\n\r\f\v") + 1);
    return normalized;
}

/* ---------- 用户系统---------- */

bool MySQLController::do_email_exist(const std::string& email) {
    std::string query = "SELECT COUNT(*) FROM users WHERE user_email = '"
        + normalize_email(email) + "';";
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
    const std::string& password_hash) {
    std::string sql = "INSERT INTO users (user_id, user_email, password_hash) VALUES ('" +
                      user_ID + "', '" + normalize_email(email)
                      + "','" + password_hash + "');";
    return execute(sql);
}

bool MySQLController::check_user_pswd(const std::string& email, const std::string& password_hash) {
    std::string sql = "SELECT COUNT(*) FROM users WHERE user_email='"
        + normalize_email(email) + "' AND password_hash='" + password_hash + "';";
    if (mysql_query(conn, sql.c_str())) {
        std::cerr << "Password check failed: " << mysql_error(conn) << std::endl;
        return false;
    }
    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) return false;
    MYSQL_ROW row = mysql_fetch_row(res);
    bool exists = (row && std::stoi(row[0]) > 0);
    mysql_free_result(res);
    return exists;
}

void MySQLController::update_user_last_active(const std::string& email) {
    std::string sql = "UPDATE users SET last_active = NOW() WHERE user_email = '"
        + normalize_email(email) + "';";
    execute(sql);
}

std::string MySQLController::get_user_id_from_email(const std::string& email) {
    std::string sql = "SELECT user_id FROM users WHERE user_email = '"
        + normalize_email(email) + "';";
    auto rows = query(sql);
    if (!rows.empty() && rows[0].size() > 0) {
        return rows[0][0];
    }
    return "";
}

/* ---------- 好友 & 群组 ---------- */

/* ---------- 聊天记录 ---------- */

/* ---------- 文件 ---------- */