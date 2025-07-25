#pragma once

#include <string>
#include <initializer_list>

namespace ansi {
    constexpr const char* RESET = "\033[0m";

    // 样式
    constexpr const char* BOLD = "\033[1m";
    constexpr const char* ITALIC = "\033[3m";
    constexpr const char* UNDERLINE = "\033[4m";

    // 前景色
    constexpr const char* FG_GREEN = "\033[32m";
    constexpr const char* FG_BRIGHT_BLUE = "\033[94m";
    constexpr const char* FG_WHITE = "\033[97m";
    constexpr const char* FG_GRAY = "\033[90m";
    constexpr const char* FG_RED = "\033[31m";
    constexpr const char* FG_YELLOW = "\033[33m";
    constexpr const char* FG_BRIGHT_RED = "\033[91m";
    constexpr const char* FG_BRIGHT_YELLOW = "\033[93m";

    // 背景色
    constexpr const char* BG_RED = "\033[41m";
}

std::string style(const std::string& text, std::initializer_list<const char*> styles);

// 一些固定用法

void sclear();

void print_input_sign();

std::string selnum(int num);

using Style = std::initializer_list<const char*>;

