# 文件目录
~~~
ChatRoom/
    README.md
    CMakeLists.txt
    project/
        global/
            include/...
            abstract/...
            entity/...
            其他实现
        io/
            include/...
            local/...
            net/...
        server/
            include/...
            chat/... (业务逻辑)
            CMakeLists.txt
            servermain.cpp (main)
            服务器主体定义
            数据库(redis)
        client/
            include/...
            chat/... (业务逻辑)
            CMakeLists.txt
            clientmain.cpp (main)
            客户端主体定义(后端)
            interface/
                include/...
                (config.json ...)
                textbased/... (纯文字界面)
                graphical/...
    .gitignore
    .vscode/...
    .git/...
    build/...
    bin/ (可执行文件)
        server
        client
~~~

# 通信类型
- 命令(command), 包括用户发送信息(核心功能)以外的操作
- 信息(message), 包括文本信息, 文件, 特殊信息(后续添加, 如卡片信息)
- 以外的主动或被动拉取的信息(data), 包括用户信息, 群聊信息, 好友消息等

# Client和Server如何通信？
父client管理
- command client
- message client
- data client

父server管理
- command server
- message server
- data server

mc <--> ms
cc ---> cs ---> ds ---> dc

增加心跳检测，实时显示连接状态(设计待定)

# 项目需求
### 账号管理
- 登录、注册、注销
- 修改密码、**找回密码**
- **(邮箱)验证码注册、找回**
- (del)**数据加密**

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
- **用户自定义在线状态(伪装)**
- 在线发送文件
- **在现用户发送文件，离线用户登录后查看离线文件**
- (del)**文件断点续传**
- 支持多种消息类型，文本、(del)**图片**、文件等
- (del)**Emoji**
- 用户在线时，收到实时消息通知(收到信息、好友申请、群聊申请等)

### 其他
- 使用Redis/LevelDB
- **实现图形界面**
- **使用Json/Proto序列化/反序列化数据库的数据**
- 撰写用户文档
- 支持大量客户端同时访问
- 实现服务器日志
- C/S双端均支持在CLI/Web上自行指定IP:Port
- 防止用户非法输入
- TCP心跳检测
- 界面美观

# 实现
