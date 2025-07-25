#include "../include/sqlite.hpp"
#include "../../global/include/logging.hpp"
#include <filesystem>
#include <stdexcept>
#include <sstream>
#include <cstdlib>

namespace {
    /**
     * @brief 展开路径中的波浪号（~）为用户主目录
     * @param path 可能包含~的路径
     * @return 展开后的绝对路径
     */
    std::string expand_path(const std::string& path) {
        if (path.empty() || path[0] != '~') {
            return path;
        }

        const char* home = std::getenv("HOME");
        if (!home) {
            // 如果没有HOME环境变量，尝试使用用户主目录
            home = std::getenv("USERPROFILE"); // Windows
            if (!home) {
                throw std::runtime_error("Cannot determine home directory");
            }
        }

        if (path.length() == 1) {
            // 只有一个~
            return std::string(home);
        } else if (path[1] == '/') {
            // ~/something 格式
            return std::string(home) + path.substr(1);
        } else {
            // ~username 格式，暂不支持，直接返回原路径
            return path;
        }
    }
}

SQLiteController::SQLiteController(const std::string& database_path)
    : db(nullptr), db_path(expand_path(database_path)) {
    // 确保目录存在
    std::filesystem::path db_file(db_path);
    std::filesystem::create_directories(db_file.parent_path());
}

SQLiteController::~SQLiteController() {
    disconnect();
}

bool SQLiteController::connect() {
    std::lock_guard<std::mutex> lock(db_mutex);

    try {
        // 使用 SQLiteCpp 创建数据库连接
        db = std::make_unique<SQLite::Database>(db_path, SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE);
        log_info("SQLite database connected: {}", db_path);

        // 启用外键约束
        db->exec("PRAGMA foreign_keys = ON;");

        // 初始化表结构
        return init_tables();
    } catch (const std::exception& e) {
        log_error("Failed to open SQLite database: {}", e.what());
        db.reset();
        return false;
    }
}

void SQLiteController::disconnect() {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (db) {
        db.reset();
        log_info("SQLite database disconnected");
    }
}

bool SQLiteController::is_connected() const {
    return db != nullptr;
}

bool SQLiteController::init_tables() {
    const std::string create_tables_sql = R"(
        CREATE TABLE IF NOT EXISTS users (
            user_id VARCHAR(30) PRIMARY KEY NOT NULL,
            user_email VARCHAR(255) NOT NULL UNIQUE,
            password_hash CHAR(60) NOT NULL
        );

        CREATE TABLE IF NOT EXISTS friends (
            user_id TEXT NOT NULL,
            friend_id TEXT NOT NULL,
            is_blocked BOOLEAN DEFAULT FALSE,
            PRIMARY KEY(user_id, friend_id),
            FOREIGN KEY(user_id) REFERENCES users(user_id)
        );

        CREATE TABLE IF NOT EXISTS chat_groups (
            group_id VARCHAR(30) PRIMARY KEY NOT NULL,
            group_name VARCHAR(255) NOT NULL,
            owner_id VARCHAR(30) NOT NULL
        );

        CREATE TABLE IF NOT EXISTS group_members (
            group_id VARCHAR(30) NOT NULL,
            user_id VARCHAR(30) NOT NULL,
            is_admin BOOLEAN DEFAULT FALSE,
            PRIMARY KEY(group_id, user_id),
            FOREIGN KEY(group_id) REFERENCES chat_groups(group_id)
        );

        CREATE TABLE IF NOT EXISTS chat_messages (
            message_id INTEGER PRIMARY KEY AUTOINCREMENT,
            sender_id VARCHAR(30) NOT NULL,
            receiver_id VARCHAR(30) NOT NULL,
            is_group BOOLEAN NOT NULL,
            timestamp INTEGER NOT NULL,
            text TEXT,
            pin BOOLEAN DEFAULT FALSE,
            file_name VARCHAR(255),
            file_size INTEGER,
            file_hash VARCHAR(128),
            synced BOOLEAN DEFAULT TRUE
        );

        CREATE INDEX IF NOT EXISTS idx_messages_receiver_time ON chat_messages(receiver_id, is_group, timestamp);
        CREATE INDEX IF NOT EXISTS idx_messages_sender_time ON chat_messages(sender_id, timestamp);
        CREATE INDEX IF NOT EXISTS idx_friends_user ON friends(user_id);
        CREATE INDEX IF NOT EXISTS idx_group_members_group ON group_members(group_id);
    )";

    return execute(create_tables_sql);
}

bool SQLiteController::execute(const std::string& query) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!db) {
        log_error("Database not connected");
        return false;
    }

    try {
        db->exec(query);
        return true;
    } catch (const std::exception& e) {
        log_error("SQL execution failed: {} - Query: {}", e.what(), query);
        return false;
    }
}

std::vector<std::vector<std::string>> SQLiteController::query(const std::string& sql) {
    std::lock_guard<std::mutex> lock(db_mutex);
    std::vector<std::vector<std::string>> results;

    if (!db) {
        log_error("Database not connected");
        return results;
    }

    try {
        SQLite::Statement query_stmt(*db, sql);

        int columns = query_stmt.getColumnCount();

        while (query_stmt.executeStep()) {
            std::vector<std::string> row;

            for (int i = 0; i < columns; i++) {
                if (query_stmt.isColumnNull(i)) {
                    row.push_back("");
                } else {
                    row.push_back(query_stmt.getColumn(i).getText());
                }
            }
            results.push_back(row);
        }
    } catch (const std::exception& e) {
        log_error("SQL query failed: {} - Query: {}", e.what(), sql);
    }

    return results;
}

/* ---------- 通用辅助方法 ---------- */

template<typename... Args>
bool SQLiteController::execute_stmt(const std::string& sql, const std::string& operation, Args&&... args) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!db) {
        log_error("Database not connected");
        return false;
    }
    try {
        SQLite::Statement stmt(*db, sql);
        bind_params(stmt, 1, std::forward<Args>(args)...);
        stmt.exec();
        return true;
    } catch (const std::exception& e) {
        log_error("{} failed: {}", operation, e.what());
        return false;
    }
}

template<typename T>
std::vector<T> SQLiteController::query_list(const std::string& sql, const std::string& operation,
                                           std::function<T(SQLite::Statement&)> mapper) {
    std::vector<T> results;
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!db) {
        log_error("Database not connected");
        return results;
    }
    try {
        SQLite::Statement stmt(*db, sql);
        while (stmt.executeStep()) {
            results.push_back(mapper(stmt));
        }
    } catch (const std::exception& e) {
        log_error("{} failed: {}", operation, e.what());
    }
    return results;
}

template<typename T, typename... Args>
std::vector<T> SQLiteController::query_list_with_params(const std::string& sql, const std::string& operation,
                                                       std::function<T(SQLite::Statement&)> mapper, Args&&... args) {
    std::vector<T> results;
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!db) {
        log_error("Database not connected");
        return results;
    }
    try {
        SQLite::Statement stmt(*db, sql);
        bind_params(stmt, 1, std::forward<Args>(args)...);
        while (stmt.executeStep()) {
            results.push_back(mapper(stmt));
        }
    } catch (const std::exception& e) {
        log_error("{} failed: {}", operation, e.what());
    }
    return results;
}

template<typename T, typename... Args>
T SQLiteController::query_single(const std::string& sql, const std::string& operation, T default_value,
                                std::function<T(SQLite::Statement&)> mapper, Args&&... args) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!db) {
        log_error("Database not connected");
        return default_value;
    }
    try {
        SQLite::Statement stmt(*db, sql);
        bind_params(stmt, 1, std::forward<Args>(args)...);
        if (stmt.executeStep()) {
            return mapper(stmt);
        }
    } catch (const std::exception& e) {
        log_error("{} failed: {}", operation, e.what());
    }
    return default_value;
}

void SQLiteController::bind_params(SQLite::Statement& stmt, int index) {
    // 递归终止条件
}

template<typename T, typename... Args>
void SQLiteController::bind_params(SQLite::Statement& stmt, int index, T&& first, Args&&... rest) {
    stmt.bind(index, std::forward<T>(first));
    bind_params(stmt, index + 1, std::forward<Args>(rest)...);
}

/* ---------- 用户系统---------- */

bool SQLiteController::store_user_info(const std::string& user_ID, const std::string& email, const std::string& password_hash) {
    return execute_stmt("INSERT OR REPLACE INTO users (user_id, user_email, password_hash) VALUES (?, ?, ?)",
                       "Store user info", user_ID, email, password_hash);
}

bool SQLiteController::get_stored_user_info(std::string& user_ID, std::string& email) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!db) {
        log_error("Database not connected");
        return false;
    }
    try {
        SQLite::Statement stmt(*db, "SELECT user_id, user_email FROM users LIMIT 1");
        if (stmt.executeStep()) {
            user_ID = stmt.getColumn(0).getText();
            email = stmt.getColumn(1).getText();
            return true;
        }
    } catch (const std::exception& e) {
        log_error("Failed to get user info: {}", e.what());
    }
    return false;
}

bool SQLiteController::clear_user_info() {
    return execute("DELETE FROM users");
}

/* ---------- 好友 ---------- */

bool SQLiteController::cache_friend(const std::string& user_ID, const std::string& friend_ID, bool is_blocked) {
    return execute_stmt("INSERT OR REPLACE INTO friends (user_id, friend_id, is_blocked) VALUES (?, ?, ?)",
                       "Cache friend", user_ID, friend_ID, is_blocked ? 1 : 0);
}

bool SQLiteController::remove_friend_cache(const std::string& user_ID, const std::string& friend_ID) {
    return execute_stmt("DELETE FROM friends WHERE user_id = ? AND friend_id = ?",
                       "Remove friend cache", user_ID, friend_ID);
}

bool SQLiteController::update_friend_block_status(const std::string& user_ID, const std::string& friend_ID, bool is_blocked) {
    return execute_stmt("UPDATE friends SET is_blocked = ? WHERE user_id = ? AND friend_id = ?",
                       "Update friend block status", is_blocked ? 1 : 0, user_ID, friend_ID);
}

std::vector<std::string> SQLiteController::get_cached_friends_list(const std::string& user_ID) {
    return query_list_with_params<std::string>(
        "SELECT friend_id FROM friends WHERE user_id = ? AND is_blocked = 0",
        "Get friends list",
        [](SQLite::Statement& stmt) { return stmt.getColumn(0).getText(); },
        user_ID
    );
}

std::vector<std::pair<std::string, bool>> SQLiteController::get_cached_friends_with_block_status(const std::string& user_ID) {
    return query_list_with_params<std::pair<std::string, bool>>(
        "SELECT friend_id, is_blocked FROM friends WHERE user_id = ?",
        "Get friends with block status",
        [](SQLite::Statement& stmt) {
            return std::make_pair(stmt.getColumn(0).getText(), stmt.getColumn(1).getInt() != 0);
        },
        user_ID
    );
}

/* ---------- 群组 ---------- */

bool SQLiteController::cache_group(const std::string& group_ID, const std::string& group_name, const std::string& owner_ID) {
    return execute_stmt("INSERT OR REPLACE INTO chat_groups (group_id, group_name, owner_id) VALUES (?, ?, ?)",
                       "Cache group", group_ID, group_name, owner_ID);
}

bool SQLiteController::cache_group_member(const std::string& group_ID, const std::string& user_ID, bool is_admin) {
    return execute_stmt("INSERT OR REPLACE INTO group_members (group_id, user_id, is_admin) VALUES (?, ?, ?)",
                       "Cache group member", group_ID, user_ID, is_admin ? 1 : 0);
}

bool SQLiteController::remove_group_cache(const std::string& group_ID) {
    execute_stmt("DELETE FROM group_members WHERE group_id = ?", "Remove group members", group_ID);
    return execute_stmt("DELETE FROM chat_groups WHERE group_id = ?", "Remove group", group_ID);
}

bool SQLiteController::remove_group_member_cache(const std::string& group_ID, const std::string& user_ID) {
    return execute_stmt("DELETE FROM group_members WHERE group_id = ? AND user_id = ?",
                       "Remove group member", group_ID, user_ID);
}

bool SQLiteController::update_group_admin_status(const std::string& group_ID, const std::string& user_ID, bool is_admin) {
    return execute_stmt("UPDATE group_members SET is_admin = ? WHERE group_id = ? AND user_id = ?",
                       "Update group admin status", is_admin ? 1 : 0, group_ID, user_ID);
}

std::vector<std::string> SQLiteController::get_cached_user_groups(const std::string& user_ID) {
    return query_list_with_params<std::string>(
        "SELECT group_id FROM group_members WHERE user_id = ?",
        "Get user groups",
        [](SQLite::Statement& stmt) { return stmt.getColumn(0).getText(); },
        user_ID
    );
}

std::vector<std::pair<std::string, bool>> SQLiteController::get_cached_group_members_with_admin_status(const std::string& group_ID) {
    return query_list_with_params<std::pair<std::string, bool>>(
        "SELECT user_id, is_admin FROM group_members WHERE group_id = ?",
        "Get group members with admin status",
        [](SQLite::Statement& stmt) {
            return std::make_pair(stmt.getColumn(0).getText(), stmt.getColumn(1).getInt() != 0);
        },
        group_ID
    );
}

std::string SQLiteController::get_cached_group_owner(const std::string& group_ID) {
    return query_single<std::string>(
        "SELECT owner_id FROM chat_groups WHERE group_id = ?",
        "Get group owner",
        "",
        [](SQLite::Statement& stmt) { return stmt.getColumn(0).getText(); },
        group_ID
    );
}

std::string SQLiteController::get_cached_group_name(const std::string& group_ID) {
    return query_single<std::string>(
        "SELECT group_name FROM chat_groups WHERE group_id = ?",
        "Get group name",
        "",
        [](SQLite::Statement& stmt) { return stmt.getColumn(0).getText(); },
        group_ID
    );
}

/* ---------- 聊天记录缓存 ---------- */

bool SQLiteController::cache_chat_message(
    const std::string& sender_ID,
    const std::string& receiver_ID,
    bool is_group,
    std::time_t timestamp,
    const std::string& text_content,
    bool pin,
    const std::string& file_name,
    std::size_t file_size,
    const std::string& file_hash) {

    std::lock_guard<std::mutex> lock(db_mutex);
    if (!db) {
        log_error("Database not connected");
        return false;
    }

    try {
        SQLite::Statement stmt(*db,
            "INSERT INTO chat_messages (sender_id, receiver_id, is_group, timestamp, text, pin, file_name, file_size, file_hash) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");

        stmt.bind(1, sender_ID);
        stmt.bind(2, receiver_ID);
        stmt.bind(3, is_group ? 1 : 0);
        stmt.bind(4, (int64_t)timestamp);
        stmt.bind(5, text_content);
        stmt.bind(6, pin ? 1 : 0);

        // 绑定文件相关字段，如果没有文件则为NULL
        if (pin && !file_name.empty()) {
            stmt.bind(7, file_name);
            stmt.bind(8, (int64_t)file_size);
            stmt.bind(9, file_hash);
        } else {
            stmt.bind(7);  // NULL
            stmt.bind(8);  // NULL
            stmt.bind(9);  // NULL
        }

        stmt.exec();
        return true;
    } catch (const std::exception& e) {
        log_error("Failed to cache chat message: {}", e.what());
        return false;
    }
}

std::vector<std::vector<std::string>> SQLiteController::get_private_chat_history(
    const std::string& user_A,
    const std::string& user_B,
    int limit) {

    std::stringstream sql;
    sql << "SELECT sender_id, receiver_id, timestamp, text, pin, file_name, file_size, file_hash "
        << "FROM chat_messages "
        << "WHERE is_group = 0 "
        << "AND ((sender_id = '" << user_A << "' AND receiver_id = '" << user_B << "') OR "
        << "(sender_id = '" << user_B << "' AND receiver_id = '" << user_A << "')) "
        << "ORDER BY timestamp ASC "
        << "LIMIT " << limit << ";";

    return query(sql.str());
}

std::vector<std::vector<std::string>> SQLiteController::get_group_chat_history(
    const std::string& group_ID,
    int limit) {

    std::stringstream sql;
    sql << "SELECT sender_id, receiver_id, timestamp, text, pin, file_name, file_size, file_hash "
        << "FROM chat_messages "
        << "WHERE is_group = 1 "
        << "AND receiver_id = '" << group_ID << "' "
        << "ORDER BY timestamp ASC "
        << "LIMIT " << limit << ";";

    return query(sql.str());
}

bool SQLiteController::clear_old_messages(int days_to_keep) {
    std::time_t cutoff_time = std::time(nullptr) - (days_to_keep * 24 * 60 * 60);
    std::string sql = "DELETE FROM chat_messages WHERE timestamp < " + std::to_string(cutoff_time) + ";";
    return execute(sql);
}

/* ---------- 离线消息管理 ---------- */

bool SQLiteController::mark_message_as_synced(long long message_id) {
    return execute_stmt("UPDATE chat_messages SET synced = 1 WHERE message_id = ?",
                       "Mark message as synced", (int64_t)message_id);
}

std::vector<std::vector<std::string>> SQLiteController::get_unsynced_messages() {
    return query_list<std::vector<std::string>>(
        "SELECT message_id, sender_id, receiver_id, is_group, timestamp, text, pin, file_name, file_size, file_hash "
        "FROM chat_messages WHERE synced = 0 ORDER BY timestamp ASC",
        "Get unsynced messages",
        [](SQLite::Statement& stmt) {
            std::vector<std::string> row;
            for (int i = 0; i < stmt.getColumnCount(); i++) {
                if (stmt.isColumnNull(i)) {
                    row.push_back("");
                } else {
                    row.push_back(stmt.getColumn(i).getText());
                }
            }
            return row;
        }
    );
}