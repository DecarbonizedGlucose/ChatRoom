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

bool MySQLController::update_user_status(const std::string& user_ID, bool online) {
    std::string status = online ? "active" : "offline";
    std::string sql = "UPDATE users SET status = '" + status + "' WHERE user_id = '" + user_ID + "';";
    return execute(sql);
}

/* ---------- 好友 ---------- */

bool MySQLController::add_friend(const std::string& user_ID, const std::string& friend_ID) {
    std::string sql = "INSERT INTO friends (user_id, friend_id) VALUES ('"
        + user_ID + "', '" + friend_ID + "');";
    return execute(sql);
}

bool MySQLController::delete_friend(const std::string& user_ID, const std::string& friend_ID) {
    std::string sql = "DELETE FROM friends WHERE user_id = '"
        + user_ID + "' AND friend_id = '" + friend_ID + "';";
    return execute(sql);
}

bool MySQLController::block_friend(const std::string& user_ID, const std::string& friend_ID) {
    std::string sql = "UPDATE friends SET is_blocked = TRUE WHERE user_id = '"
        + user_ID + "' AND friend_id = '" + friend_ID + "';";
    return execute(sql);
}

bool MySQLController::unblock_friend(const std::string& user_ID, const std::string& friend_ID) {
    std::string sql = "UPDATE friends SET is_blocked = FALSE WHERE user_id = '"
        + user_ID + "' AND friend_id = '" + friend_ID + "';";
    return execute(sql);
}

bool MySQLController::is_friend(const std::string& user_ID, const std::string& friend_ID) {
    std::string sql = "SELECT COUNT(*) FROM friends WHERE user_id = '"
        + user_ID + "' AND friend_id = '" + friend_ID + "';";
    auto rows = query(sql);
    if (!rows.empty() && rows[0].size() > 0) {
        return std::stoi(rows[0][0]) > 0;
    }
    return false;
}

bool MySQLController::search_user(const std::string& searched_ID) {
    std::string sql = "SELECT COUNT(*) FROM users WHERE user_id = '" + searched_ID + "';";
    auto rows = query(sql);
    if (!rows.empty() && rows[0].size() > 0) {
        return std::stoi(rows[0][0]) > 0;
    }
    return false;
}

std::vector<std::string> MySQLController::get_friends_list(const std::string& user_ID) {
    std::vector<std::string> friends;
    std::string sql = "SELECT friend_id FROM friends WHERE user_id = '" + user_ID + "' AND is_blocked = FALSE;";
    auto rows = query(sql);
    for (const auto& row : rows) {
        if (!row.empty()) {
            friends.push_back(row[0]);
        }
    }
    return friends;
}

std::vector<std::pair<std::string, bool>> MySQLController::get_friends_with_block_status(const std::string& user_ID) {
    std::vector<std::pair<std::string, bool>> friends;
    std::string sql = "SELECT friend_id, is_blocked FROM friends WHERE user_id = '" + user_ID + "';";
    auto rows = query(sql);
    for (const auto& row : rows) {
        if (row.size() >= 2) {
            bool is_blocked = (row[1] == "1" || row[1] == "TRUE");
            friends.emplace_back(row[0], is_blocked);
        }
    }
    return friends;
}

/* ---------- 群组 ---------- */

bool MySQLController::create_group(const std::string& group_ID,
                                  const std::string& group_name,
                                  const std::string& owner_ID) {
    // 1. 创建群组
    std::string sql = "INSERT INTO chat_groups (group_id, group_name, owner_id) VALUES ('"
        + group_ID + "', '" + group_name + "', '" + owner_ID + "');";
    if (!execute(sql)) {
        return false;
    }

    // 2. 将群主添加到群成员表中
    std::string member_sql = "INSERT INTO group_members (group_id, user_id, is_admin) VALUES ('"
        + group_ID + "', '" + owner_ID + "', FALSE);";
    return execute(member_sql);
}

bool MySQLController::add_user_to_group(const std::string& group_ID,
                                       const std::string& user_ID) {
    std::string sql = "INSERT INTO group_members (group_id, user_id, is_admin) VALUES ('"
        + group_ID + "', '" + user_ID + "', FALSE);";
    return execute(sql);
}

bool MySQLController::kick_user_from_group(const std::string& group_ID,
                                          const std::string& user_ID) {
    // kick_user_from_group 和 remove_user_from_group 实现相同
    return remove_user_from_group(group_ID, user_ID);
}

bool MySQLController::remove_user_from_group(const std::string& group_ID,
                                            const std::string& user_ID) {
    std::string sql = "DELETE FROM group_members WHERE group_id = '"
        + group_ID + "' AND user_id = '" + user_ID + "';";
    return execute(sql);
}

bool MySQLController::disband_group(const std::string& group_ID) {
    // 1. 删除所有群成员
    std::string members_sql = "DELETE FROM group_members WHERE group_id = '" + group_ID + "';";
    execute(members_sql); // 即使失败也继续执行下一步

    // 2. 删除群组
    std::string group_sql = "DELETE FROM chat_groups WHERE group_id = '" + group_ID + "';";
    return execute(group_sql);
}

bool MySQLController::search_group(const std::string& group_ID) {
    std::string sql = "SELECT COUNT(*) FROM chat_groups WHERE group_id = '" + group_ID + "';";
    auto rows = query(sql);
    if (!rows.empty() && rows[0].size() > 0) {
        return std::stoi(rows[0][0]) > 0;
    }
    return false;
}

bool MySQLController::is_user_in_group(const std::string& group_ID, const std::string& user_ID) {
    std::string sql = "SELECT COUNT(*) FROM group_members WHERE group_id = '"
        + group_ID + "' AND user_id = '" + user_ID + "';";
    auto rows = query(sql);
    if (!rows.empty() && rows[0].size() > 0) {
        return std::stoi(rows[0][0]) > 0;
    }
    return false;
}

bool MySQLController::add_group_admin(const std::string& group_ID,
                                     const std::string& user_ID) {
    std::string sql = "UPDATE group_members SET is_admin = TRUE WHERE group_id = '"
        + group_ID + "' AND user_id = '" + user_ID + "';";
    return execute(sql);
}

bool MySQLController::remove_group_admin(const std::string& group_ID,
                                        const std::string& user_ID) {
    std::string sql = "UPDATE group_members SET is_admin = FALSE WHERE group_id = '"
        + group_ID + "' AND user_id = '" + user_ID + "';";
    return execute(sql);
}

std::vector<std::string> MySQLController::get_user_groups(const std::string& user_ID) {
    std::vector<std::string> groups;
    std::string sql = "SELECT group_id FROM group_members WHERE user_id = '" + user_ID + "';";
    auto rows = query(sql);
    for (const auto& row : rows) {
        if (!row.empty()) {
            groups.push_back(row[0]);
        }
    }
    return groups;
}

std::vector<std::pair<std::string, bool>> MySQLController::get_group_members_with_admin_status(const std::string& group_ID) {
    std::vector<std::pair<std::string, bool>> members;
    std::string sql = "SELECT user_id, is_admin FROM group_members WHERE group_id = '" + group_ID + "';";
    auto rows = query(sql);
    for (const auto& row : rows) {
        if (row.size() >= 2) {
            bool is_admin = (row[1] == "1" || row[1] == "TRUE");
            members.emplace_back(row[0], is_admin);
        }
    }
    return members;
}

std::string MySQLController::get_group_owner(const std::string& group_ID) {
    std::string sql = "SELECT owner_id FROM chat_groups WHERE group_id = '" + group_ID + "';";
    auto rows = query(sql);
    if (!rows.empty() && rows[0].size() > 0) {
        return rows[0][0];
    }
    return "";
}

std::string MySQLController::get_group_name(const std::string& group_ID) {
    std::string sql = "SELECT group_name FROM chat_groups WHERE group_id = '" + group_ID + "';";
    auto rows = query(sql);
    if (!rows.empty() && rows[0].size() > 0) {
        return rows[0][0];
    }
    return "";
}

/* ---------- 聊天记录 ---------- */

bool MySQLController::add_chat_message(
    const std::string& sender_ID,
    const std::string& receiver_ID,
    bool is_to_group,
    std::time_t timestamp,
    const std::string& text_content,
    bool pin,
    const std::string& file_name,
    const std::size_t file_size,
    const std::string& file_hash) {

    std::string sql = "INSERT INTO chat_messages (sender_id, receiver_id, is_group, timestamp, text, pin, file_name, file_size, file_hash) VALUES ('"
        + sender_ID + "', '" + receiver_ID + "', "
        + (is_to_group ? "TRUE" : "FALSE") + ", " + std::to_string(timestamp) + ", '"
        + text_content + "', " + (pin ? "TRUE" : "FALSE");

    if (!file_name.empty()) {
        sql += ", '" + file_name + "', " + std::to_string(file_size) + ", '" + file_hash + "'";
    } else {
        sql += ", NULL, NULL, NULL";
    }

    sql += ");";
    return execute(sql);
}

/* ---------- 文件 ---------- */