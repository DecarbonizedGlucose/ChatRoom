# One ChatRoom 聊天室项目
这是一段话不知道写什么
有好几行
### 功能
### 运行环境
### 构建
arch/manjaro:
~~~sh
yay -S openssl
yay -S protobuf                                # 序列化/反序列化
yay -S redis-plus-plus                         # redis
yay -S mysql-connector-c++                     # mysql
sudo pacman -S nlohmann-json                   # json
yay -S curl                                    # 邮件
sudo pacman -S abseil-cpp
yay -S spdlog                                  # 日志
yay -S sqlite                                  # sqlite
~~~
debian/ubuntu:
~~~sh
sudo apt install openssl
sudo apt install protobuf-compiler libprotobuf-dev
sudo apt install libhiredis-dev libssl-dev
sudo apt install libredis++-dev
sudo apt install mysql-server mysql-common mysql-client
sudo apt install libmysqlcppconn-dev
sudo apt install nlohmann-json3-dev
sudo apt-get install libcurl4-openssl-dev
~~~