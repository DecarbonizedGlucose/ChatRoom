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

# 可选：如果系统没有 cmake，则安装（某些环境未预装）
if ! command -v cmake >/dev/null 2>&1; then
    sudo apt install -y cmake
fi

# 封装更健壮的 SQLiteCpp 检测
has_sqlitecpp() {
    # 1) pkg-config（常见大小写两种）
    if pkg-config --exists SQLiteCpp 2>/dev/null || pkg-config --exists sqlitecpp 2>/dev/null; then
        return 0
    fi
    # 2) 头文件路径
    for inc in \
        /usr/include/SQLiteCpp/SQLiteCpp.h \
        /usr/local/include/SQLiteCpp/SQLiteCpp.h \
        /opt/homebrew/include/SQLiteCpp/SQLiteCpp.h \
        /opt/local/include/SQLiteCpp/SQLiteCpp.h; do
        [ -f "$inc" ] && return 0
    done
    # 3) CMake config
    for cfg in \
        /usr/lib/cmake/SQLiteCpp/SQLiteCppConfig.cmake \
        /usr/lib64/cmake/SQLiteCpp/SQLiteCppConfig.cmake \
        /usr/local/lib/cmake/SQLiteCpp/SQLiteCppConfig.cmake \
        /usr/local/lib64/cmake/SQLiteCpp/SQLiteCppConfig.cmake; do
        [ -f "$cfg" ] && return 0
    done
    # 4) 已安装的 deb 包
    if dpkg -s libsqlitecpp-dev >/dev/null 2>&1; then
        return 0
    fi
    return 1
}

if has_sqlitecpp; then
    echo "SQLiteCpp 已安装，跳过安装"
else
    echo "尝试通过 apt 安装 SQLiteCpp (libsqlitecpp-dev)…"
    if sudo apt install -y libsqlitecpp-dev >/dev/null 2>&1; then
        echo "已通过 apt 安装 SQLiteCpp"
    else
        echo "apt 未提供或安装失败，回退到源码构建 SQLiteCpp…"
        cd /tmp
        rm -rf SQLiteCpp
        git clone --depth=1 https://github.com/SRombauts/SQLiteCpp.git
        cd SQLiteCpp && mkdir -p build && cd build
        cmake .. && make -j"$(nproc)" && sudo make install
        cd /tmp
        rm -rf SQLiteCpp
    fi
fi

PROTOBUF_VERSION=3.21.12
PROTOBUF_PREFIX="$HOME/.local/protobuf-${PROTOBUF_VERSION}"

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
    make install
    echo "Protobuf ${PROTOBUF_VERSION} 安装完成。"
    cd /tmp
    rm -rf protobuf-${PROTOBUF_VERSION} protobuf-all-${PROTOBUF_VERSION}.tar.gz
fi

# 确保 CMake 用到正确的版本
export CMAKE_PREFIX_PATH="${PROTOBUF_PREFIX}:$CMAKE_PREFIX_PATH"
export PATH="${PROTOBUF_PREFIX}/bin:$PATH"
export LD_LIBRARY_PATH="${PROTOBUF_PREFIX}/lib:$LD_LIBRARY_PATH"

echo "已将 Protobuf ${PROTOBUF_VERSION} 安装至 ${PROTOBUF_PREFIX}。"

cd "$ORIGINAL_DIR"

echo "依赖安装完成，正在构建项目..."

mkdir -p build && cd build
cmake .. -DBUILD_CLIENT_ONLY=true
make -j$(nproc)

echo "构建完成，正在初始化环境..."

mkdir -p ~/.local/share/ChatRoom
cd ~/.local/share/ChatRoom
mkdir -p log

echo "正在初始化本地数据库..."
sqlite3 ./chat.db < "$ORIGINAL_DIR/sql/client_init.sql"

echo "客户端所有文件已安装到 ~/.local/share/ChatRoom"

echo "数据库安装完成!"

cd "$ORIGINAL_DIR/bin"

echo "客户端文件已安装到 $ORIGINAL_DIR/bin/client"
