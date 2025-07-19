#include "../../include/CLI/TerminalInput.hpp"
#include <iostream>
#include <termios.h>
#include <unistd.h>

TerminalInput::TerminalInput() : running(false) {}

TerminalInput::~TerminalInput() {
    stop();
}

void TerminalInput::start() {
    running = true;
    input_thread = std::thread(&TerminalInput::input_loop, this);
}

void TerminalInput::stop() {
    running = false;
    if (input_thread.joinable()) {
        input_thread.join();
    }
}

void TerminalInput::set_func(char ch, func cb) {
    std::lock_guard<std::mutex> lock(mtx);
    key_cb[ch] = std::move(cb);
}

void TerminalInput::clear_func(char ch) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = key_cb.find(ch);
    if (it != key_cb.end()) {
        key_cb.erase(it);
    }
}

void TerminalInput::clear_key_func() {
    std::lock_guard<std::mutex> lock(mtx);
    key_cb.clear();
}

void TerminalInput::clear_buffer() {
    std::lock_guard<std::mutex> lock(mtx);
    input_buffer.clear();
}

void TerminalInput::clear_enter_func() {
    std::lock_guard<std::mutex> lock(mtx);
    enter_cb = nullptr;
}

void TerminalInput::clear_all_func() {
    std::lock_guard<std::mutex> lock(mtx);
    key_cb.clear();
    enter_cb = nullptr;
}

void TerminalInput::print_message(const std::string& msg) {
    std::lock_guard<std::mutex> lock(mtx);
    std::cout << "\r\033[K" << msg << "\n";  // 清除当前行后输出消息
    redraw_input_line();                      // 重新绘制输入行
}

void TerminalInput::wait() {
    if (input_thread.joinable()) {
        input_thread.join();
    }
}

void TerminalInput::set_enter_callback(std::function<void(const std::string&)> cb) {
    std::lock_guard<std::mutex> lock(mtx);
    enter_cb = std::move(cb);
}

void TerminalInput::set_enable_display(bool enable) {
    std::lock_guard<std::mutex> lock(mtx);
    enable_display = enable;
}

std::string TerminalInput::get_buffer() const {
    return input_buffer;
}

bool TerminalInput::buffer_empty() const {
    return input_buffer.empty();
}

void TerminalInput::redraw_input_line() {
    std::cout << "\r> " << input_buffer << "\033[K" << std::flush;
}

void TerminalInput::input_loop() {
    // 设置终端为原始模式，逐字读取输入
    termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    char ch;
    while (running && std::cin.get(ch)) {
        std::lock_guard<std::mutex> lock(mtx);

        if (enable_display && (ch == 127 || ch == 8)) { // 退格
            if (!input_buffer.empty()) {
                input_buffer.pop_back();
            }
        } else if (key_cb.find(ch) != key_cb.end()) {
            key_cb[ch]();
        } else if (enable_display && isprint(ch)) {
            input_buffer += ch;
        }
        if (enable_display && (ch == '\n' || ch == '\r')) { // 回车提交
            if (key_cb.find(ch) != key_cb.end()) {
                enter_cb(input_buffer);
            }
            input_buffer.clear();
        }
        if (enable_display) {
            redraw_input_line();
        }
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // 恢复终端模式
}
