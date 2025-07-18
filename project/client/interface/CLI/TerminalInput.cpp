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

void TerminalInput::set_callback(Callback cb) {
    std::lock_guard<std::mutex> lock(mtx);
    on_enter_callback = std::move(cb);
}

void TerminalInput::print_message(const std::string& msg) {
    std::lock_guard<std::mutex> lock(mtx);
    std::cout << "\r\033[K" << msg << "\n";  // 清除当前行后输出消息
    redraw_input_line();                      // 重新绘制输入行
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

        if (ch == 127 || ch == 8) { // 退格
            if (!input_buffer.empty()) {
                input_buffer.pop_back();
            }
        } else if (ch == '\n' || ch == '\r') { // 回车提交
            if (on_enter_callback) {
                on_enter_callback(input_buffer);
            }
            input_buffer.clear();
        } else if (isprint(ch)) {
            input_buffer += ch;
        }

        redraw_input_line();
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // 恢复终端模式
}
