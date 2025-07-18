#include "include/TopClient.hpp"
#include "../global/include/logging.hpp"

int main() {
    set_addr_c::fetch_addr_from_config();
    TopClient client;
    client.launch();
    return 0;
}