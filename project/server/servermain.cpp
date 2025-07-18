#include "include/TopServer.hpp"
#include "include/TcpServer.hpp"
#include "../global/include/logging.hpp"

int main() {
    set_addr_s::fetch_addr_from_config();
    TopServer server;
    server.launch();
    return 0;
}