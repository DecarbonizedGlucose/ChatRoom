#include "include/TopClient.hpp"
#include "include/TcpClient.hpp"
#include "../global/include/logging.hpp"

int main() {
    spdlog::set_level(spdlog::level::debug);
    set_addr_c::fetch_addr_from_config();
    TopClient client;
    client.launch();
    return 0;
}