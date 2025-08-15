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
    std::string sql = "UPDATE users SET last_active = NOW(6) WHERE user_email = '"
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

std::string MySQLController::get_user_email_from_id(const std::string& user_ID) {
    std::string sql = "SELECT user_email FROM users WHERE user_id = '" + user_ID + "';";
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

bool MySQLController::delete_user(const std::string& user_ID) {
    // 删用户表
    std::string sql = "DELETE FROM users WHERE user_id = '" + user_ID + "';";
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

bool MySQLController::is_blocked_by_friend(const std::string& user_ID, const std::string& friend_ID) {
    std::string sql = "SELECT is_blocked FROM friends WHERE user_id = '"
        + user_ID + "' AND friend_id = '" + friend_ID + "';";
    auto rows = query(sql);
    if (!rows.empty() && rows[0].size() > 0) {
        return (rows[0][0] == "1" || rows[0][0] == "TRUE");
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

std::string MySQLController::create_group(
    const std::string& group_name,
    const std::string& owner_ID) {
    // 1. 查询现有群组ID中的最大数字，生成新的群组ID
    std::string sql = "SELECT COALESCE(MAX(CAST(SUBSTRING(group_id, 7) AS UNSIGNED)), -1) + 1 FROM chat_groups WHERE group_id REGEXP '^Group_[0-9]+$';";
    auto rows = query(sql);
    std::string next_id;
    if (rows.empty() || rows[0].empty()) {
        next_id = "0";
    } else {
        next_id = rows[0][0];
    }
    std::string group_ID = "Group_" + next_id;

    // 2. 占领该Id, 保存
    sql = "INSERT INTO chat_groups (group_id, group_name, owner_id) VALUES ('"
        + group_ID + "', '" + group_name + "', '" + owner_ID + "');";
    if (!execute(sql)) {
        return ""; // 创建失败
    }

    // 3. 将群主添加到群成员表中
    add_user_to_group(group_ID, owner_ID);
    // 4. 设为管理员
    add_group_admin(group_ID, owner_ID);

    return group_ID;
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
    std::int64_t timestamp,
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

std::vector<std::tuple<std::string, std::string, bool, std::int64_t, std::string, bool, std::string, std::size_t, std::string>>
MySQLController::get_offline_messages(const std::string& user_ID, std::int64_t last_active_time, int limit) {
    std::vector<std::tuple<std::string, std::string, bool, std::int64_t, std::string, bool, std::string, std::size_t, std::string>> messages;

    // 构建SQL查询 - 获取用户离线期间收到的消息，但只包括当前关系网内的消息
    std::string sql = "("
        // 私聊消息：只包括当前好友发给他的消息（排除已删除的好友）
        "SELECT cm.sender_id, cm.receiver_id, cm.is_group, cm.timestamp, cm.text, cm.pin, "
        "COALESCE(cm.file_name, '') as file_name, COALESCE(cm.file_size, 0) as file_size, "
        "COALESCE(cm.file_hash, '') as file_hash "
        "FROM chat_messages cm "
        "INNER JOIN friends f ON cm.sender_id = f.friend_id AND f.user_id = '" + user_ID + "' "
        "WHERE cm.receiver_id = '" + user_ID + "' AND cm.is_group = FALSE "
        "AND cm.timestamp > " + std::to_string(last_active_time) + " "
        ") UNION ("
        // 群聊消息：只包括用户当前所在群组的消息（排除已退出的群聊）
        "SELECT cm.sender_id, cm.receiver_id, cm.is_group, cm.timestamp, cm.text, cm.pin, "
        "COALESCE(cm.file_name, '') as file_name, COALESCE(cm.file_size, 0) as file_size, "
        "COALESCE(cm.file_hash, '') as file_hash "
        "FROM chat_messages cm "
        "INNER JOIN group_members gm ON cm.receiver_id = gm.group_id "
        "WHERE gm.user_id = '" + user_ID + "' AND cm.is_group = TRUE "
        "AND cm.sender_id != '" + user_ID + "' "
        "AND cm.timestamp > " + std::to_string(last_active_time) + " "
        ") ORDER BY timestamp DESC LIMIT " + std::to_string(limit) + ";";

    auto rows = query(sql);
    for (const auto& row : rows) {
        if (row.size() >= 9) {
            messages.emplace_back(
                row[0],                              // sender_id
                row[1],                              // receiver_id
                row[2] == "1" || row[2] == "TRUE",   // is_group
                std::stoll(row[3]),                  // timestamp
                row[4],                              // text
                row[5] == "1" || row[5] == "TRUE",   // pin
                row[6],                              // file_name
                std::stoull(row[7]),                 // file_size
                row[8]                               // file_hash
            );
        }
    }

    return messages;
}

std::int64_t MySQLController::get_user_last_active(const std::string& user_ID) {
    std::string sql = "SELECT UNIX_TIMESTAMP(last_active) FROM users WHERE user_id = '" + user_ID + "';";
    auto rows = query(sql);
    if (!rows.empty() && rows[0].size() > 0 && !rows[0][0].empty()) {
        return std::stoll(rows[0][0]);
    }
    return 0; // 如果没有记录, 返回0（Unix时间戳起始点）
}

/* ---------- 文件 ---------- */

// 查询目前文件数量
int MySQLController::get_file_count() {
    std::string sql = "SELECT COUNT(*) FROM chat_files;";
    auto rows = query(sql);
    if (!rows.empty() && rows[0].size() > 0) {
        return std::stoi(rows[0][0]);
    }
    return 0;
}

bool MySQLController::add_file_record(
    const std::string& file_hash,
    const std::string& file_id,
    std::size_t file_size) {
    // 对file_hash进行基本验证（应该是64字符的十六进制字符串）
    if (file_hash.length() != 64) {
        log_error("Invalid file_hash length: {}", file_hash.length());
        return false;
    }

    std::string sql = "INSERT INTO chat_files (file_hash, file_id, file_size) VALUES ('"
        + file_hash + "', '" + file_id + "', " + std::to_string(file_size) + ");";
    return execute(sql);
}

// 查询给出的file_hash是否在数据库中存在
bool MySQLController::file_hash_exists(const std::string& file_hash) {
    // 对file_hash进行基本验证（应该是64字符的十六进制字符串）
    if (file_hash.length() != 64) {
        log_error("Invalid file_hash length: {}", file_hash.length());
        return false;
    }

    std::string sql = "SELECT 1 FROM chat_files WHERE file_hash = '" + file_hash + "' LIMIT 1;";
    auto rows = query(sql);
    return !rows.empty();
}

// 通过file_hash给出file_id
std::string MySQLController::get_file_id_by_hash(const std::string& file_hash) {
    if (file_hash.length() != 64) {
        log_error("Invalid file_hash length: {}", file_hash.length());
        return "";
    }

    std::string sql = "SELECT file_id FROM chat_files WHERE file_hash = '" + file_hash + "';";
    auto rows = query(sql);
    if (!rows.empty() && rows[0].size() > 0) {
        return rows[0][0];
    }
    return ""; // 未找到
}

// 通过file_id给出file_hash
std::string MySQLController::get_file_hash_by_id(const std::string& file_id) {
    std::string sql = "SELECT file_hash FROM chat_files WHERE file_id = '" + file_id + "';";
    auto rows = query(sql);
    if (!rows.empty() && rows[0].size() > 0) {
        return rows[0][0];
    }
    return ""; // 未找到
}

// 通过file_id获取文件信息(文件名和大小)
std::optional<std::pair<std::string, size_t>> MySQLController::get_file_info(const std::string& file_id) {
    // 首先从chat_files表获取file_hash和file_size
    std::string sql = "SELECT file_hash, file_size FROM chat_files WHERE file_id = '" + file_id + "';";
    auto rows = query(sql);
    if (rows.empty() || rows[0].size() < 2) {
        log_error("File not found in chat_files table for file_id: {}", file_id);
        return std::nullopt;
    }

    std::string file_hash = rows[0][0];
    size_t file_size = std::stoull(rows[0][1]);

    return std::make_pair(file_hash, file_size);
}

// 通过file_hash生成新的file_id
// 仅生成file_id（不插入数据库）
std::string MySQLController::generate_file_id_only(const std::string& file_hash) {
    // 验证file_hash格式
    if (file_hash.length() != 64) {
        log_error("Invalid file_hash length: {}", file_hash.length());
        return "";
    }

    // 首先检查file_hash是否已存在
    if (file_hash_exists(file_hash)) {
        log_info("File hash {} already exists in database", file_hash);
        return ""; // file_hash已存在，返回空字符串
    }

    // 获取当前文件数量并生成新的file_id
    int file_count = get_file_count();
    std::string new_file_id = "File_" + std::to_string(file_count);

    log_info("Generated new file_id: {} for hash: {}", new_file_id, file_hash);
    return new_file_id;
}

// 如果file_hash存在，返回"", 反之，返回 "File_" + std::tostring(num), num是已存在的文件数量
std::string MySQLController::generate_file_id_by_hash(const std::string& file_hash, std::size_t file_size) {
    // 验证file_hash格式
    if (file_hash.length() != 64) {
        log_error("Invalid file_hash length: {}", file_hash.length());
        return "";
    }

    // 首先检查file_hash是否已存在
    if (file_hash_exists(file_hash)) {
        log_info("File hash {} already exists in database", file_hash);
        return ""; // file_hash已存在，返回空字符串
    }

    // 获取当前文件数量
    int file_count = get_file_count();

    // 生成新的file_id
    std::string new_file_id = "File_" + std::to_string(file_count);

    // 插入新记录
    std::string sql = "INSERT INTO chat_files (file_hash, file_id, file_size) VALUES ('" +
                      file_hash + "', '" + new_file_id + "', " + std::to_string(file_size) + ");";

    if (execute(sql)) {
        log_info("Created new file record: file_id={}, file_hash={}, size={}", new_file_id, file_hash, file_size);
        return new_file_id;
    } else {
        log_error("Failed to create new file record for hash: {}", file_hash);
        return "";
    }
}

/* ---------- 通知/请求 ---------- */

int MySQLController::store_command(const CommandRequest& cmd, bool managed) {
    std::string sql = "INSERT INTO chat_commands (action, sender, para1, para2, para3, para4, para5, para6, managed) VALUES ("
        + std::to_string(static_cast<int>(cmd.action())) + ", '"
        + cmd.sender() + "', '"
        + (cmd.args_size() > 0 ? cmd.args(0) : "") + "', '"
        + (cmd.args_size() > 1 ? cmd.args(1) : "") + "', '"
        + (cmd.args_size() > 2 ? cmd.args(2) : "") + "', '"
        + (cmd.args_size() > 3 ? cmd.args(3) : "") + "', '"
        + (cmd.args_size() > 4 ? cmd.args(4) : "") + "', '"
        + (cmd.args_size() > 5 ? cmd.args(5) : "") + "', "
        + (managed ? "TRUE" : "FALSE") + ");";

    if (execute(sql)) {
        return mysql_insert_id(conn); // 返回新插入记录的ID
    } else {
        return -1;
    }
}

bool MySQLController::delete_command(int command_id) {
    std::string sql = "DELETE FROM chat_commands WHERE id = " + std::to_string(command_id) + ";";
    if (execute(sql)) {
        return true;
    } else {
        return false;
    }
}

bool MySQLController::update_command_status(int command_id, bool managed) {
    std::string sql = "UPDATE chat_commands SET managed = "
        + (managed ? std::string("TRUE") : std::string("FALSE")) + " WHERE id = " + std::to_string(command_id) + ";";
    return execute(sql);
}

int MySQLController::get_command_status(int command_id) {
    std::string sql = "SELECT managed FROM chat_commands WHERE id = " + std::to_string(command_id) + ";";
    // 执行查询并返回结果
    if (execute(sql)) {
        // 查询成功，获取结果
        MYSQL_RES* result = mysql_store_result(conn);
        if (result) {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row) {
                bool managed = (row[0] && row[0][0] == '1');
                mysql_free_result(result);
                return managed;
            }
            mysql_free_result(result);
        }
    }
    return -1;
}

CommandRequest MySQLController::get_command(int command_id) {
    std::string sql = "SELECT action, sender, para1, para2, para3, para4, para5, para6 FROM chat_commands WHERE id = " + std::to_string(command_id) + ";";
    auto rows = query(sql);
    if (!rows.empty() && rows[0].size() >= 8) {
        CommandRequest cmd;
        cmd.set_action(std::stoi(rows[0][0]));
        cmd.set_sender(rows[0][1]);
        if (!rows[0][2].empty()) cmd.add_args(rows[0][2]);  // para1
        if (!rows[0][3].empty()) cmd.add_args(rows[0][3]);  // para2
        if (!rows[0][4].empty()) cmd.add_args(rows[0][4]);  // para3
        if (!rows[0][5].empty()) cmd.add_args(rows[0][5]);  // para4
        if (!rows[0][6].empty()) cmd.add_args(rows[0][6]);  // para5
        if (!rows[0][7].empty()) cmd.add_args(rows[0][7]);  // para6
        return cmd;
    }
    return CommandRequest();
}

bool MySQLController::add_pending_command(const std::string& user_id, int command_id) {
    std::string sql = "INSERT INTO user_pending_commands (user_id, command_id) VALUES ('"
        + user_id + "', " + std::to_string(command_id) + ");";
    return execute(sql);
}

bool MySQLController::remove_pending_command(const std::string& user_id, int command_id) {
    std::string sql = "DELETE FROM user_pending_commands WHERE user_id = '"
        + user_id + "' AND command_id = " + std::to_string(command_id) + ";";
    return execute(sql);
}

std::vector<CommandRequest> MySQLController::get_pending_commands(const std::string& user_id) {
    std::vector<CommandRequest> commands;
    std::string sql = "SELECT command_id FROM user_pending_commands WHERE user_id = '" + user_id + "';";
    auto rows = query(sql);
    for (const auto& row : rows) {
        if (!row.empty()) {
            int command_id = std::stoi(row[0]);
            CommandRequest cmd = get_command(command_id);
            commands.push_back(cmd);
        }
    }
    return commands;
}

bool MySQLController::clear_pending_commands(const std::string& user_id) {
    std::string sql = "DELETE FROM user_pending_commands WHERE user_id = '" + user_id + "';";
    bool result = execute(sql);
    log_debug("Cleared all pending commands for user: {}", user_id);
    return result;
}

bool MySQLController::add_command_to_all_admin(
    int command_id,
    const std::string& group_ID
) {
    std::string sql = "INSERT INTO user_pending_commands (user_id, command_id) "
                      "SELECT user_id, " + std::to_string(command_id) + " "
                      "FROM group_members "
                      "WHERE group_id = '" + group_ID + "' AND is_admin = TRUE;";
    bool result = execute(sql);
    return result;
}

bool MySQLController::add_command_to_all_admin_except(
    const std::string& user_ID,
    int command_id,
    const std::string& group_ID
) {
    std::string sql = "INSERT INTO user_pending_commands (user_id, command_id) "
                      "SELECT user_id, " + std::to_string(command_id) + " "
                      "FROM group_members "
                      "WHERE group_id = '" + group_ID + "' AND is_admin = TRUE "
                      "AND user_id != '" + user_ID + "';";
    bool result = execute(sql);
    return result;
}

bool MySQLController::remove_command_from_all_admin(
    int command_id,
    const std::string& group_ID
) {
    std::string sql = "DELETE upc FROM user_pending_commands upc "
                      "INNER JOIN group_members gm ON upc.user_id = gm.user_id "
                      "WHERE upc.command_id = " + std::to_string(command_id) + " "
                      "AND gm.group_id = '" + group_ID + "' AND gm.is_admin = TRUE;";
    bool result = execute(sql);
    return result;
}

