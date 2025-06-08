# 文件目录
~~~
ChatRoom/
    README.md
    CMakeLists.txt
    project/
        global/
            include/
                threadpool.hpp
                command.hpp 用户行为命令
                message.hpp
            abstract/
                command.cpp
            entity/
                message.cpp
            其他实现
        io/
            include/
                reactor.hpp
                socket.hpp
            local/
            net/
                include/
                reactor.cpp
                socket封装
        server/
            include/
            CMakeLists.txt
            server.cpp (main)
            服务器主体定义
            数据库(redis)
        client/
            include/
            CMakeLists.txt
            client.cpp (main)
            客户端主体定义(后端)
            interface/
                include/
                config.json
                textbased/ (纯文字界面)
                graphical/
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

# Client和Server如何通信？
父client管理
- command client
- message client

父server管理
- command server
- message server

增加心跳检测，实时显示连接状态
