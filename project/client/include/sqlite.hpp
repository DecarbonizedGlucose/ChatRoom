#pragma once
#include <SQLiteCpp/SQLiteCpp.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <ctime>
#include <functional>

/**
 * @brief SQLite数据库控制器类
 *
 * 负责客户端本地数据缓存管理，包括用户信息、好友关系、群组数据、
 * 聊天记录和离线消息的存储与查询。使用SQLiteCpp库实现线程安全的数据库操作。
 */
class SQLiteController {
private:
    std::unique_ptr<SQLite::Database> db;  // SQLite数据库连接对象
    std::mutex db_mutex;                   // 数据库操作互斥锁，确保线程安全
    std::string db_path;                   // 数据库文件路径

    // 初始化数据库表结构
    bool init_tables();

    /* ---------- 通用辅助模板方法 ---------- */
    // 这些模板方法用于简化重复的数据库操作代码

    /**
     * @brief 执行带参数的SQL语句（INSERT/UPDATE/DELETE）
     * @param sql SQL语句，使用?作为参数占位符
     * @param operation 操作描述，用于错误日志
     * @param args 可变参数列表，自动绑定到SQL占位符
     * @return 操作是否成功
     */
    template<typename... Args>
    bool execute_stmt(const std::string& sql, const std::string& operation, Args&&... args);

    /**
     * @brief 查询返回列表，无参数
     * @param sql SQL查询语句
     * @param operation 操作描述
     * @param mapper 结果映射函数，将SQLite::Statement转换为目标类型
     * @return 查询结果列表
     */
    template<typename T>
    std::vector<T> query_list(const std::string& sql, const std::string& operation,
                             std::function<T(SQLite::Statement&)> mapper);

    /**
     * @brief 查询返回列表，带参数
     * @param sql SQL查询语句，使用?作为参数占位符
     * @param operation 操作描述
     * @param mapper 结果映射函数
     * @param args 可变参数列表
     * @return 查询结果列表
     */
    template<typename T, typename... Args>
    std::vector<T> query_list_with_params(const std::string& sql, const std::string& operation,
                                         std::function<T(SQLite::Statement&)> mapper, Args&&... args);

    /**
     * @brief 查询单个值，带参数
     * @param sql SQL查询语句
     * @param operation 操作描述
     * @param default_value 默认返回值（查询无结果时）
     * @param mapper 结果映射函数
     * @param args 可变参数列表
     * @return 查询结果或默认值
     */
    template<typename T, typename... Args>
    T query_single(const std::string& sql, const std::string& operation, T default_value,
                  std::function<T(SQLite::Statement&)> mapper, Args&&... args);

    /**
     * @brief 递归绑定SQL参数的终止条件
     */
    void bind_params(SQLite::Statement& stmt, int index);

    /**
     * @brief 递归绑定SQL参数
     * @param stmt SQLite预编译语句对象
     * @param index 当前参数索引
     * @param first 当前参数值
     * @param rest 剩余参数
     */
    template<typename T, typename... Args>
    void bind_params(SQLite::Statement& stmt, int index, T&& first, Args&&... rest);

public:
    /**
     * @brief 构造函数
     * @param database_path 数据库文件路径，会自动创建目录
     * @note 支持路径格式：
     *       - 绝对路径："/home/user/chat.db"
     *       - 相对路径："./data/chat.db"
     *       - 波浪号路径："~/.local/share/ChatRoom/chat.db"
     *       - 仅波浪号："~" (表示用户主目录)
     */
    SQLiteController(const std::string& database_path);

    /**
     * @brief 析构函数，自动断开数据库连接
     */
    ~SQLiteController();

    /**
     * @brief 连接数据库并初始化表结构
     * @return 连接是否成功
     */
    bool connect();

    /**
     * @brief 断开数据库连接
     */
    void disconnect();

    /**
     * @brief 检查数据库连接状态
     * @return 是否已连接
     */
    bool is_connected() const;

    /* ---------- 通用数据库操作 ---------- */

    /**
     * @brief 执行简单SQL语句（不带参数）
     * @param query SQL语句
     * @return 执行是否成功
     */
    bool execute(const std::string& query);

    /**
     * @brief 执行查询并返回字符串结果集
     * @param sql SQL查询语句
     * @return 查询结果，每行为一个字符串向量
     */
    std::vector<std::vector<std::string>> query(const std::string& sql);

/* ---------- 用户系统 ---------- */

    /**
     * @brief 存储用户登录信息到本地缓存
     * @param user_ID 用户ID
     * @param email 用户邮箱
     * @param password_hash 密码哈希值
     * @return 存储是否成功
     * @note 用于实现自动登录功能，密码哈希应已加密
     */
    bool store_user_info(
        const std::string& user_ID,
        const std::string& email,
        const std::string& password_hash);

    /**
     * @brief 获取已缓存的用户信息
     * @param user_ID 输出参数：用户ID
     * @param email 输出参数：用户邮箱
     * @return 是否成功获取用户信息
     * @note 用于自动登录时验证本地缓存的用户信息
     */
    bool get_stored_user_info(std::string& user_ID, std::string& email);

    /**
     * @brief 清除本地用户信息缓存
     * @return 清除是否成功
     * @note 用于用户注销登录时清理本地数据
     */
    bool clear_user_info();

/* ---------- 好友系统 ---------- */

    /**
     * @brief 缓存好友关系
     * @param user_ID 用户ID
     * @param friend_ID 好友ID
     * @param is_blocked 是否被拉黑，默认为false
     * @return 缓存是否成功
     * @note 用于同步服务器的好友关系到本地缓存
     */
    bool cache_friend(const std::string& user_ID, const std::string& friend_ID, bool is_blocked = false);

    /**
     * @brief 删除好友关系缓存
     * @param user_ID 用户ID
     * @param friend_ID 要删除的好友ID
     * @return 删除是否成功
     * @note 用于删除好友时清理本地缓存
     */
    bool remove_friend_cache(const std::string& user_ID, const std::string& friend_ID);

    /**
     * @brief 更新好友拉黑状态
     * @param user_ID 用户ID
     * @param friend_ID 好友ID
     * @param is_blocked 新的拉黑状态
     * @return 更新是否成功
     * @note 用于拉黑或解除拉黑好友
     */
    bool update_friend_block_status(const std::string& user_ID, const std::string& friend_ID, bool is_blocked);

    /* ---------- 好友查询函数 ---------- */

    /**
     * @brief 获取未被拉黑的好友ID列表
     * @param user_ID 用户ID
     * @return 好友ID列表
     * @note 用于显示好友列表，不包含被拉黑的好友
     */
    std::vector<std::string> get_cached_friends_list(const std::string& user_ID);

    /**
     * @brief 获取所有好友及其拉黑状态
     * @param user_ID 用户ID
     * @return pair<好友ID, 是否被拉黑>的列表
     * @note 用于好友管理界面，显示所有好友及其状态
     */
    std::vector<std::pair<std::string, bool>> get_cached_friends_with_block_status(const std::string& user_ID);

/* ---------- 群组系统 ---------- */

    /**
     * @brief 缓存群组基本信息
     * @param group_ID 群组ID
     * @param group_name 群组名称
     * @param owner_ID 群主ID
     * @return 缓存是否成功
     * @note 用于同步服务器的群组信息到本地
     */
    bool cache_group(const std::string& group_ID, const std::string& group_name, const std::string& owner_ID);

    /**
     * @brief 缓存群组成员信息
     * @param group_ID 群组ID
     * @param user_ID 成员用户ID
     * @param is_admin 是否为管理员，默认为false
     * @return 缓存是否成功
     * @note 用于同步群组成员列表及其权限
     */
    bool cache_group_member(const std::string& group_ID, const std::string& user_ID, bool is_admin = false);

    /**
     * @brief 删除群组及其所有成员缓存
     * @param group_ID 群组ID
     * @return 删除是否成功
     * @note 用于解散群组时清理本地数据
     */
    bool remove_group_cache(const std::string& group_ID);

    /**
     * @brief 删除特定群组成员缓存
     * @param group_ID 群组ID
     * @param user_ID 要删除的成员ID
     * @return 删除是否成功
     * @note 用于踢出群组成员时更新本地缓存
     */
    bool remove_group_member_cache(const std::string& group_ID, const std::string& user_ID);

    /**
     * @brief 更新群组成员管理员权限
     * @param group_ID 群组ID
     * @param user_ID 成员ID
     * @param is_admin 新的管理员状态
     * @return 更新是否成功
     * @note 用于设置或取消成员管理员权限
     */
    bool update_group_admin_status(const std::string& group_ID, const std::string& user_ID, bool is_admin);

    /* ---------- 群组查询函数 ---------- */

    /**
     * @brief 获取用户加入的所有群组ID列表
     * @param user_ID 用户ID
     * @return 群组ID列表
     * @note 用于显示用户的群组列表
     */
    std::vector<std::string> get_cached_user_groups(const std::string& user_ID);

    /**
     * @brief 获取群组所有成员及其管理员状态
     * @param group_ID 群组ID
     * @return pair<成员ID, 是否为管理员>的列表
     * @note 用于群组管理界面显示成员列表
     */
    std::vector<std::pair<std::string, bool>> get_cached_group_members_with_admin_status(const std::string& group_ID);

    /**
     * @brief 获取群组群主ID
     * @param group_ID 群组ID
     * @return 群主ID，失败返回空字符串
     */
    std::string get_cached_group_owner(const std::string& group_ID);

    /**
     * @brief 获取群组名称
     * @param group_ID 群组ID
     * @return 群组名称，失败返回空字符串
     */
    std::string get_cached_group_name(const std::string& group_ID);

/* ---------- 聊天记录缓存系统 ---------- */

    /**
     * @brief 缓存聊天消息
     * @param sender_ID 发送者ID
     * @param receiver_ID 接收者ID（私聊为用户ID，群聊为群组ID）
     * @param is_group 是否为群聊消息
     * @param timestamp 消息时间戳
     * @param text_content 消息文本内容
     * @param pin 是否为文件消息，默认为false
     * @param file_name 文件名，默认为空
     * @param file_size 文件大小（字节），默认为0
     * @param file_hash 文件哈希值，默认为空
     * @return 缓存是否成功
     * @note 支持文本消息和文件消息的缓存，文件消息需设置pin=true并提供文件信息
     */
    bool cache_chat_message(
        const std::string& sender_ID,
        const std::string& receiver_ID,
        bool is_group,
        std::time_t timestamp,
        const std::string& text_content,
        bool pin = false,
        const std::string& file_name = "",
        std::size_t file_size = 0,
        const std::string& file_hash = "");

    /* ---------- 聊天记录查询 ---------- */

    /**
     * @brief 获取两个用户之间的私聊记录
     * @param user_A 用户A的ID
     * @param user_B 用户B的ID
     * @param limit 返回消息数量限制，默认100条
     * @return 聊天记录，每条消息为字符串向量
     * @note 返回格式：[sender_id, receiver_id, timestamp, text, pin, file_name, file_size, file_hash]
     *       按时间升序排列
     */
    std::vector<std::vector<std::string>> get_private_chat_history(
        const std::string& user_A,
        const std::string& user_B,
        int limit = 100);

    /**
     * @brief 获取群组聊天记录
     * @param group_ID 群组ID
     * @param limit 返回消息数量限制，默认100条
     * @return 群聊记录，每条消息为字符串向量
     * @note 返回格式同get_private_chat_history，按时间升序排列
     */
    std::vector<std::vector<std::string>> get_group_chat_history(
        const std::string& group_ID,
        int limit = 100);

    /**
     * @brief 清理超过指定天数的旧消息
     * @param days_to_keep 保留消息的天数，默认30天
     * @return 清理是否成功
     * @note 用于定期清理本地存储，节省磁盘空间
     */
    bool clear_old_messages(int days_to_keep = 30);

/* ---------- 离线消息管理系统 ---------- */

    /**
     * @brief 标记消息已与服务器同步
     * @param message_id 消息ID（本地数据库自增ID）
     * @return 标记是否成功
     * @note 用于离线消息管理，网络恢复后批量同步消息到服务器
     */
    bool mark_message_as_synced(long long message_id);

    /**
     * @brief 获取未同步的消息列表
     * @return 未同步消息列表，每条消息为字符串向量
     * @note 返回格式：[message_id, sender_id, receiver_id, is_group, timestamp, text, pin, file_name, file_size, file_hash]
     *       用于网络恢复后批量上传到服务器
     */
    std::vector<std::vector<std::string>> get_unsynced_messages();
};

