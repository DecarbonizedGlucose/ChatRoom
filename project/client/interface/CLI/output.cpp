#include "../../include/CLI/output.hpp"
#include <iostream>

void sclear() {
    std::cout << "\033[2J\033[H"; // 清屏并将光标移到左上角
}

void print_input_sign() {
    std::cout << style("=> ", {ansi::FG_BRIGHT_RED, ansi::BOLD});
}
