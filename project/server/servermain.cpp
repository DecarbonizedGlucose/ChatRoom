#include "include/TopServer.hpp"
#include "include/TcpServer.hpp"
#include "../global/include/logging.hpp"
#include <string>
#include <istream>

int main() {
    set_addr_s::fetch_addr_from_config();
    TopServer server;
    server.launch();
    std::string cmd;
    while (std::cin >> cmd) {
        if (cmd == "exit" || cmd == "quit") {
            server.stop();
            break;
        } else {
            std::cout << "Unknown command: " << cmd << std::endl;
        }
    }
    return 0;
}