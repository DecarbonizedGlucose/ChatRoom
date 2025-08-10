# ChatRoom 聊天室用户手册


## 简单介绍

一个使用C++11/17特性，基于Reactor模型的多线程TCP聊天室。



## 部署（Ubuntu）

部署时请使用ubuntu。如有其他需求，请按照脚本，根据系统特性自行配置。

~~~mysql
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

CREATE TABLE user_pending_commands ( # 保留
    user_id VARCHAR(30) NOT NULL,
    command_id INT NOT NULL,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY(user_id, command_id),
    FOREIGN KEY(user_id) REFERENCES users(user_id),
    FOREIGN KEY(command_id) REFERENCES chat_commands(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE
);
~~~





## 功能和使用




## 项目架构



## 设计细节



### Server

分为三个逻辑服务器，分别是MessageSever，CommandServer和DataServer，监听三个端口（可指定），由`TcpServer实现`。每个逻辑服务器有自己的Epoll Reactor（`Reactor`），在一定程度上相通。

`Reactor`只负责读写事件触发，业务逻辑由`Dispacther`分发。三个逻辑服务器共用一个`Dispatcher`，用来区分数据类型，以便确定业务逻辑，也有统筹管理三个逻辑服务器的功能。

ChatServer负责收发消息（`ChatMessage`），把消息暂存redis，定时批量转存到mysql以提高运作效率。定时器安装在`Dispatcher`。

CommandServer负责处理用户账户行为（登录、注册、注销），聊天室内部复杂的业务逻辑（添加好友、申请加入群聊、搜索账号等），以及TCP心跳检测的一部分，传输`CommandRequest`。具体行为在`project/global/include/action.hpp`中定义。

DataServer负责传输文件（`FileChunk`）、历史聊天记录（`OfflineMessages`）、最新的全部或部分关系网（`SyncItem`）（好友、群聊，以及群聊的成员列表）。

处于安全与便利考虑，封装了socket，并集成在`TcpServerConnection`中。显然每个用户有三个`TcpServerConnection`（不允许异地登录），这是用户连接真正的元模块。

`ConnectionManager`用于管理与用户的连接，可注册、取出用户特定的`TcpServerConnection`，以及负责管理TCP心跳检测。内部有一个表记录`user_ID`和`TcpServerConnection`的关系。用户对三个逻辑服务器的任何发送都可以更新心跳，当用户长时间无响应，CommandServer会发送心跳包。心跳检测的`last_active`记录在redis中。

封装了`Socket`，解决了非阻塞模式下的TCP粘包、半包问题。

实时输出日志。

### Client

设计较简单，分为3个逻辑客户端，由`TcpClient`实现，功能同上。

`WinLoop`用于创建循环展示CLI，`CommunicationManager`负责缓存数据，管理会话，分发逻辑等功能。客户端也有线程池。

使用SQLite存储聊天记录、用户关系网等信息。客户端不与服务器的数据库连接。

输出日志到本地文件。

### 注册

用户需使用邮箱注册。本项目的服务器采用了QQ邮箱，部署服务器时在redis中设置好邮箱地址和授权码。

用户注册时会收到6位验证码，5分钟失效。

### 文件传输

本项目对文件传输有特殊处理。用户发送文件时，会先在本地求hash，发送至服务器。若hash已经存在，则客户端不用再次发送文件主体，只在消息中写入文件名和大小等元数据（类似telegram）。

传输时，对文件进行分片操作，一片一片传输，每片4MB，不长时间占用某线程，支持多用户同时传输。这样之后还可以对项目进行扩展设计，实现断点续传。

传输完成再次求hash，以验证文件完整性。



## 关于项目

目录结构：

~~~
.
├── CMakeLists.txt
├── install.sh
├── project
│   ├── client
│   │   ├── chat
│   │   │   ├── cfile_manager.cpp
│   │   │   ├── CommManager.cpp
│   │   │   ├── init_db.sql
│   │   │   └── sqlite.cpp
│   │   ├── clientmain.cpp
│   │   ├── CMakeLists.txt
│   │   ├── include
│   │   │   ├── cfile_manager.hpp
│   │   │   ├── CommManager.hpp
│   │   │   ├── output.hpp
│   │   │   ├── sqlite.hpp
│   │   │   ├── TcpClient.hpp
│   │   │   ├── TopClient.hpp
│   │   │   └── winloop.hpp
│   │   ├── interface
│   │   │   ├── output.cpp
│   │   │   └── winloop.cpp
│   │   ├── TcpClient.cpp
│   │   └── TopClient.cpp
│   ├── global
│   │   ├── abstract
│   │   │   ├── command.pb.cc
│   │   │   ├── command.pb.h
│   │   │   ├── data.pb.cc
│   │   │   ├── data.pb.h
│   │   │   ├── datatypes.cpp
│   │   │   ├── datatypes.hpp
│   │   │   ├── envelope.pb.cc
│   │   │   ├── envelope.pb.h
│   │   │   ├── message.pb.cc
│   │   │   ├── message.pb.h
│   │   │   └── proto
│   │   │       ├── command.proto
│   │   │       ├── data.proto
│   │   │       ├── envelope.proto
│   │   │       └── message.proto
│   │   ├── CMakeLists.txt
│   │   ├── entity
│   │   │   └── file.cpp
│   │   └── include
│   │       ├── action.hpp
│   │       ├── command.hpp
│   │       ├── file.hpp
│   │       ├── group.hpp
│   │       ├── logging.hpp
│   │       ├── safe_deque.hpp
│   │       ├── safe_queue.hpp
│   │       ├── threadpool.hpp
│   │       ├── time_utils.hpp
│   │       └── user.hpp
│   ├── io
│   │   ├── CMakeLists.txt
│   │   ├── include
│   │   │   ├── ioaction.hpp
│   │   │   ├── reactor.hpp
│   │   │   └── Socket.hpp
│   │   └── net
│   │       ├── ioaction.cpp
│   │       ├── reactor.cpp
│   │       └── Socket.cpp
│   └── server
│       ├── chat
│       │   ├── dispatcher.cpp
│       │   ├── email.cpp
│       │   ├── handler.cpp
│       │   └── sfile_manager.cpp
│       ├── CMakeLists.txt
│       ├── connection_manager.cpp
│       ├── database
│       │   ├── mysql.cpp
│       │   └── redis.cpp
│       ├── include
│       │   ├── connection_manager.hpp
│       │   ├── dispatcher.hpp
│       │   ├── email.hpp
│       │   ├── handler.hpp
│       │   ├── mysql.hpp
│       │   ├── redis.hpp
│       │   ├── sfile_manager.hpp
│       │   ├── TcpServerConnection.hpp
│       │   ├── TcpServer.hpp
│       │   └── TopServer.hpp
│       ├── servermain.cpp
│       ├── TcpServerConnection.cpp
│       ├── TcpServer.cpp
│       └── TopServer.cpp
└── README.md

18 directories, 75 files
~~~
项目C++代码近9000行。`protobuf`文件也编译为`.cc``.h`，这些已排除在外。
~~~
-------------------------------------------------------------------------------
Language                     files          blank        comment           code
-------------------------------------------------------------------------------
C++                             24            854            589           7100
C/C++ Header                    31            449            239           1842
-------------------------------------------------------------------------------
SUM:                            55           1303            828           8942
-------------------------------------------------------------------------------
~~~

