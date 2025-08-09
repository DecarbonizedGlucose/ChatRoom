#include "include/TopClient.hpp"
#include "include/TcpClient.hpp"
#include "../global/include/logging.hpp"
#include <filesystem>
#include <spdlog/sinks/rotating_file_sink.h>

int main(int argc, char* argv[]) {
    if (argc == 5) {
        std::string addr = argv[1];
        uint16_t port1 = std::stoi(argv[2]);
        uint16_t port2 = std::stoi(argv[3]);
        uint16_t port3 = std::stoi(argv[4]);
        set_addr_c::client_addr[0] = {addr, port1};
        set_addr_c::client_addr[1] = {addr, port2};
        set_addr_c::client_addr[2] = {addr, port3};
    }
    // 创建日志目录
    std::filesystem::create_directories(std::getenv("HOME") + std::string("/.local/share/ChatRoom/log/"));

    try {
        // 配置日志输出到文件而不是终端, 避免与UI冲突
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            std::getenv("HOME") + std::string("/.local/share/ChatRoom/log/client.log"), 1024*1024*5, 3); // 5MB, 3个文件

        auto logger = std::make_shared<spdlog::logger>("client", file_sink);
        logger->set_level(spdlog::level::debug);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        spdlog::set_default_logger(logger);

        log_info("ChatRoom client starting...");

        TopClient client;
        client.launch();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}