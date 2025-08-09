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

CREATE TABLE chat_messages (
    message_id INTEGER PRIMARY KEY AUTOINCREMENT,
    sender_id VARCHAR(30) NOT NULL,
    receiver_id VARCHAR(30) NOT NULL,
    is_group BOOLEAN NOT NULL,
    timestamp INTEGER NOT NULL,
    text TEXT,
    pin BOOLEAN DEFAULT FALSE,
    file_name VARCHAR(255),
    file_size INTEGER,
    file_hash VARCHAR(128)
);
