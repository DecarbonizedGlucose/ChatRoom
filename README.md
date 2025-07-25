# One ChatRoom 聊天室项目
这是一段话不知道写什么
有好几行
### 功能
### 运行环境
### 构建
#### 依赖(Ubuntu/Debian系)
~~~sh
sudo apt install -y \
  libcurl4-openssl-dev \
  libssl-dev \
  protobuf-compiler \
  libprotobuf-dev \
  libmysqlclient-dev \
  nlohmann-json3-dev \
  libspdlog-dev \
  libabsl-dev \
  libncurses-dev \
  libhiredis-dev \
  build-essential \
  libsqlite3-dev

# SQLiteCpp 需要手动编译安装
cd /tmp
git clone https://github.com/SRombauts/SQLiteCpp.git
cd SQLiteCpp && mkdir build && cd build
cmake .. && make && sudo make install
# redis++ 需要手动编译安装
cd /tmp
git clone https://github.com/sewenew/redis-plus-plus.git
cd redis-plus-plus
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
sudo make install
~~~