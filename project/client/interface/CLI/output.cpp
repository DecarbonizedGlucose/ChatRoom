#include "../../include/CLI/output.hpp"
#include <iostream>

std::string style(const std::string& text, std::initializer_list<const char*> styles) {
    std::string result;
    for (const auto& style : styles) {
        result += style;
    }
    result += text + ansi::RESET;
    return result;
}

void sclear() {
    std::cout << "\033[2J\033[H"; // 清屏并将光标移到左上角
}

void print_input_sign() {
    std::cout << style("=> ", {ansi::FG_BRIGHT_RED, ansi::BOLD});
}

std::string selnum(int num) {
    return style("[" + std::to_string(num) + "]", {ansi::FG_BRIGHT_BLUE, ansi::BOLD});
}