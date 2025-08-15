# ChatRoom 聊天室用户手册


## 简单介绍

一个使用C++11/17特性，基于Reactor模型的多线程TCP聊天室。



## 部署客户端

运行`install.sh`。



## 功能和使用

业务逻辑参考QQ、微信、telegram等热门即时通讯软件。

在聊天界面使用`/up file_path`发送文件，用`/down File_ID local_path/file.name`下载文件。
其他操作按照导航提示即可。




## 项目架构

![proj](https://github.com/user-attachments/assets/f1eb35b2-65ae-45db-9416-01ed7aa19271)



## 设计细节

### Server

分为三个逻辑服务器，分别是MessageSever，CommandServer和DataServer，监听三个端口（可指定），由`TcpServer`实现。每个逻辑服务器有自己的Epoll Reactor（`Reactor`），在一定程度上相通。

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
│   │   │   ├── output.cpp
│   │   │   ├── sqlite.cpp
│   │   │   └── winloop.cpp
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
│   │       ├── logging.hpp
│   │       ├── safe_deque.hpp
│   │       ├── safe_queue.hpp
│   │       ├── threadpool.hpp
│   │       └── time_utils.hpp
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
├── README.md
└── sql
    ├── client_init.sql
    └── server_init.sql

18 directories, 74 files
~~~
项目C++代码9400行。`protobuf`文件也编译为`.cc` `.h`，这些已排除在外。
~~~
--------------------------------------------------------------------
Language          files          blank        comment           code
--------------------------------------------------------------------
C++                  24            884            684           7615
C/C++ Header         29            441            219           1825
--------------------------------------------------------------------
SUM:                 53           1325            903           9435
--------------------------------------------------------------------
~~~

