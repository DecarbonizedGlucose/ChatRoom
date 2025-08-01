#include "include/TcpClient.hpp"

namespace set_addr_c {
    Addr client_addr[3];
    bool fetch_addr_from_config() {
        // 模拟配置, 以后再改
        client_addr[0] = {"127.0.0.1", 9527};
        client_addr[1] = {"127.0.0.1", 9528};
        client_addr[2] = {"127.0.0.1", 9529};
        return true;
    }
}