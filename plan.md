# 项目需求
### 账号管理
- 登录、注册、注销
- 修改密码、**找回密码**
- **(邮箱)验证码注册、找回**

### 好友管理
- 添加、删除、查询好友
- 显示好友在线状态
- 禁止不存在好友关系的用户间私聊
- 实现屏蔽私信
- 好友间聊天

### 群聊
- 创建、解散群聊
- 用户申请加入
- 用户查看已经加入的群聊
- 用户退出群聊
- 用户查看群聊成员
- 群主添加、删除管理员
- 群主/管理员批准/拒绝用户加入
- 群主/管理员踢人
- 群内聊天

### 聊天功能
- 查看历史聊天记录
- 用户在线聊天
- 在线用户对离线用户发送消息
- 离线用户登录后查看离线消息
- 在线发送文件
- **在现用户发送文件，离线用户登录后查看离线文件**
- 支持多种消息类型，文本、文件等
- 用户在线时，收到实时消息通知(收到信息、好友申请、群聊申请等)

### 其他
- 使用Redis/LevelDB
- **(done)使用Json/Proto序列化/反序列化数据库的数据**
- 撰写用户文档
- 支持大量客户端同时访问
- 实现服务器日志
- C/S双端均支持在CLI/Web上自行指定IP:Port
- 防止用户非法输入
- TCP心跳检测
- 界面美观

# 实现
### 客户端api列表
- 文字输入
- 选择文件
- 发送/接收各种东西
- 查看历史记录
- 获取关系网（含在线状态）
    - 查看好友列表
    - 查看群聊列表
    - 查看所在群聊的成员列表
- 获取用户基本信息
- 获取群聊基本信息
- 文件hash校验

## 聊天室数据库设计与消息系统整理

### ✅ 用户信息表
```sql
CREATE TABLE users (
    user_id VARCHAR(30) PRIMARY KEY NOT NULL,
    user_email VARCHAR(255) NOT NULL UNIQUE,
    password_hash CHAR(60) NOT NULL,
    last_active TIMESTAMP NULL,
    status ENUM('active', 'offline') DEFAULT 'offline'
);
```

### ✅ 好友列表表
```sql
CREATE TABLE friends (
    user_id VARCHAR(30) NOT NULL,
    friend_id VARCHAR(30) NOT NULL,
    is_blocked BOOLEAN DEFAULT FALSE,
    PRIMARY KEY(user_id, friend_id),
    FOREIGN KEY(user_id) REFERENCES users(user_id),
    FOREIGN KEY(friend_id) REFERENCES users(user_id)
);
```

### ✅ 群组表（群元数据）
```sql
CREATE TABLE chat_groups (
    group_id VARCHAR(30) PRIMARY KEY NOT NULL,
    group_name VARCHAR(255) NOT NULL,
    owner_id VARCHAR(30) NOT NULL,
    FOREIGN KEY(owner_id) REFERENCES users(user_id)
);
```

### ✅ 群成员表（群聊成员）
```sql
CREATE TABLE group_members (
    group_id VARCHAR(30) NOT NULL,
    user_id VARCHAR(30) NOT NULL,
    is_admin BOOLEAN DEFAULT FALSE,
    PRIMARY KEY(group_id, user_id),
    FOREIGN KEY(group_id) REFERENCES chat_groups(group_id),
    FOREIGN KEY(user_id) REFERENCES users(user_id)
);
```

- **群主=群表中 owner_id**，成员表不再单独存储是否为群主，避免冗余。

---

### ✅ 如何查找：

**从某个用户查找好友列表：**
```sql
SELECT friend_id, is_blocked FROM friends
WHERE user_id = 'xxx';
```

**从某个用户查找所在群：**
```sql
SELECT group_id FROM group_members
WHERE user_id = 'xxx';
```

**从群ID查找所有群成员：**
```sql
SELECT user_id, is_admin FROM group_members
WHERE group_id = 'group123';
```

**查找群主：**
```sql
SELECT owner_id FROM groups
WHERE group_id = 'group123';
```

### ✅ 聊天记录数据库表（统一）
```sql
CREATE TABLE chat_messages (
    message_id BIGINT AUTO_INCREMENT PRIMARY KEY,
    sender_id VARCHAR(30) NOT NULL,
    receiver_id VARCHAR(30) NOT NULL,
    is_group BOOLEAN NOT NULL,
    timestamp BIGINT NOT NULL,
    text TEXT,
    pin BOOLEAN DEFAULT FALSE,
    file_name VARCHAR(255),
    file_size BIGINT,
    file_hash VARCHAR(128),
    FOREIGN KEY (sender_id) REFERENCES users(user_id)
);
```

---

### ✅ 查询示例

**私聊：**
```sql
SELECT * FROM chat_messages
WHERE is_group = FALSE
  AND ((sender_id = 'userA' AND receiver_id = 'userB') OR
       (sender_id = 'userB' AND receiver_id = 'userA'))
ORDER BY timestamp ASC
LIMIT 100;
```

**群聊：**
```sql
SELECT * FROM chat_messages
WHERE is_group = TRUE
  AND receiver_id = 'group123'
ORDER BY timestamp ASC
LIMIT 100;
```

---

### ✅ 优化建议
- 为 `(receiver_id, is_group, timestamp)` 建复合索引。
- 文件内容存在本地或云端，数据库仅存元数据。
- 可拆分活跃消息表和归档表，支持分区。

~~~
[decglu][群主/管理员]<2025.07.25 16:35:12>
一条文本消息
第二行
第三行
~~(test.txt)[123456, 16.8KB]~~

[Soyo][2025.07.25 16:35:15]
~~(senpai.mp4)[234567, 2.5MB]~~
~~~
