#include "include/TopServer.hpp"
#include "include/TcpServer.hpp"
#include "../global/include/logging.hpp"
#include <string>
#include <istream>

int main(int argc, char* argv[]) {
    if (argc > 1) {
        mysql_config::config(argv[1]);
    }
    if (argc == 5) {
        uint16_t port1 = std::stoi(argv[2]);
        uint16_t port2 = std::stoi(argv[3]);
        uint16_t port3 = std::stoi(argv[4]);
        set_addr_s::server_addr[0] = {"0.0.0.0", port1};
        set_addr_s::server_addr[1] = {"0.0.0.0", port2};
        set_addr_s::server_addr[2] = {"0.0.0.0", port3};
    }
    spdlog::set_level(spdlog::level::debug);
    std::srand(std::time(nullptr));
    TopServer server;
    if (!server.launch()) {
        log_error("Failed to launch TopServer");
        return 1;
    }
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