#!/usr/bin/env bash
set -e
set -o pipefail

ORIGINAL_DIR=$(pwd)

echo "正在安装依赖..."

# 安装客户端依赖
sudo apt update

sudo apt install -y        \
    pkg-config             \
    libcurl4-openssl-dev   \
    libssl-dev             \
    nlohmann-json3-dev     \
    libspdlog-dev          \
    build-essential        \
    libsqlite3-dev         \
    sqlite3

if pkg-config --exists SQLiteCpp; then
    echo "SQLiteCpp 已安装，跳过安装"
else
    echo "正在安装 SQLiteCpp..."
    cd /tmp
    git clone https://github.com/SRombauts/SQLiteCpp.git
    cd SQLiteCpp && mkdir build && cd build
    cmake .. && make && sudo make install
    cd /tmp
    rm -rf SQLiteCpp
fi

PROTOBUF_VERSION=3.21.12
INSTALL_PREFIX=/opt/protobuf-${PROTOBUF_VERSION}

if [ -f "${PROTOBUF_PREFIX}/bin/protoc" ]; then
    echo "Protobuf ${PROTOBUF_VERSION} 已安装，跳过编译。"
else
    echo "正在安装 Protobuf ${PROTOBUF_VERSION}..."
    cd /tmp
    rm -rf protobuf-${PROTOBUF_VERSION}
    wget -q https://github.com/protocolbuffers/protobuf/releases/download/v${PROTOBUF_VERSION}/protobuf-all-${PROTOBUF_VERSION}.tar.gz
    tar -xzf protobuf-all-${PROTOBUF_VERSION}.tar.gz
    cd protobuf-${PROTOBUF_VERSION}
    mkdir build && cd build
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -Dprotobuf_BUILD_TESTS=OFF \
        -DCMAKE_INSTALL_PREFIX=${PROTOBUF_PREFIX}
    make -j$(nproc)
    sudo make install
    sudo ldconfig
    echo "Protobuf ${PROTOBUF_VERSION} 安装完成。"
    cd /tmp
    rm -rf protobuf-${PROTOBUF_VERSION} protobuf-all-${PROTOBUF_VERSION}.tar.gz
fi

# 确保 CMake 用到正确的版本
export CMAKE_PREFIX_PATH=${PROTOBUF_PREFIX}:$CMAKE_PREFIX_PATH
export LD_LIBRARY_PATH=${PROTOBUF_PREFIX}/lib:$LD_LIBRARY_PATH

cd "$ORIGINAL_DIR"

echo "依赖安装完成，正在构建项目..."

mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target client -j$(nproc)

echo "构建完成，正在初始化环境..."

mkdir -p ~/.local/share/ChatRoom
cd ~/.local/share/ChatRoom
mkdir -p log

echo "正在初始化本地数据库..."
sqlite3 ./chat.db < "$ORIGINAL_DIR/init_db.sql"

echo "客户端所有文件已安装到 ~/.local/share/ChatRoom"

echo "数据库安装完成!"

cd "$ORIGINAL_DIR/bin"

echo "客户端文件已安装到 $ORIGINAL_DIR/bin/client"
