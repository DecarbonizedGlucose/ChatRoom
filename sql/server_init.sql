CREATE TABLE users (
    user_id VARCHAR(30) PRIMARY KEY,
    user_email VARCHAR(255) NOT NULL UNIQUE,
    password_hash CHAR(60) NOT NULL,
    last_active TIMESTAMP NULL,
    status ENUM('active', 'offline') DEFAULT 'offline'
);

CREATE TABLE friends (
    user_id VARCHAR(30) NOT NULL,
    friend_id VARCHAR(30) NOT NULL,
    is_blocked BOOLEAN DEFAULT FALSE,
    PRIMARY KEY(user_id, friend_id),
    FOREIGN KEY(user_id) REFERENCES users(user_id),
    FOREIGN KEY(friend_id) REFERENCES users(user_id)
);

CREATE TABLE chat_groups (
    group_id VARCHAR(30) PRIMARY KEY NOT NULL,
    group_name VARCHAR(255) NOT NULL,
    owner_id VARCHAR(30) NOT NULL,
    FOREIGN KEY(owner_id) REFERENCES users(user_id)
);

CREATE TABLE group_members (
    group_id VARCHAR(30) NOT NULL,
    user_id VARCHAR(30) NOT NULL,
    is_admin BOOLEAN DEFAULT FALSE,
    PRIMARY KEY(group_id, user_id),
    FOREIGN KEY(group_id) REFERENCES chat_groups(group_id),
    FOREIGN KEY(user_id) REFERENCES users(user_id)
);

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

CREATE TABLE chat_files (
    file_hash CHAR(64) PRIMARY KEY,
    file_id VARCHAR(36) NOT NULL UNIQUE,
    file_size BIGINT NOT NULL DEFAULT 0
);

CREATE TABLE chat_commands (
    id INT AUTO_INCREMENT PRIMARY KEY,
    action TINYINT NOT NULL,
    sender VARCHAR(255) NOT NULL,
    para1 VARCHAR(255),
    para2 VARCHAR(255),
    para3 VARCHAR(255),
    para4 VARCHAR(255),
    para5 VARCHAR(255),
    para6 VARCHAR(255),
    managed BOOLEAN DEFAULT FALSE,
    FOREIGN KEY(sender) REFERENCES users(user_id)
        ON UPDATE CASCADE
        ON DELETE CASCADE
);

-- CREATE TABLE user_pending_commands ( # 保留
--     user_id VARCHAR(30) NOT NULL,
--     command_id INT NOT NULL,
--     timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
--     PRIMARY KEY(user_id, command_id),
--     FOREIGN KEY(user_id) REFERENCES users(user_id),
--     FOREIGN KEY(command_id) REFERENCES chat_commands(id)
--         ON UPDATE CASCADE
--         ON DELETE CASCADE
-- );