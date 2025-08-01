#include "include/TopClient.hpp"
#include "include/TcpClient.hpp"
#include "../global/include/logging.hpp"
#include <filesystem>
#include <spdlog/sinks/rotating_file_sink.h>

int main() {
    // 创建日志目录
    std::filesystem::create_directories("/tmp/chatroom");

    try {
        // 配置日志输出到文件而不是终端, 避免与UI冲突
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "/tmp/chatroom/client.log", 1024*1024*5, 3); // 5MB, 3个文件

        auto logger = std::make_shared<spdlog::logger>("client", file_sink);
        logger->set_level(spdlog::level::debug);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        spdlog::set_default_logger(logger);

        log_info("ChatRoom client starting...");

        set_addr_c::fetch_addr_from_config();
        TopClient client;
        client.launch();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}