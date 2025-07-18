#pragma once

#include <functional>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

class TerminalInput {
public:
    using Callback = std::function<void(const std::string&)>;

    TerminalInput();
    ~TerminalInput();

    void start();
    void stop();
    void set_callback(Callback cb);               // 设置回车时的回调函数
    void print_message(const std::string& msg);   // 输出外部消息，不干扰输入

private:
    std::thread input_thread;                     // 单开一个线程
    std::mutex mtx;
    std::string input_buffer;
    Callback on_enter_callback;                   // 回车后的回调
    std::atomic<bool> running;

    void redraw_input_line();                     // 重新绘制输入行
    void input_loop();                            // 输入循环，处理输入和回车
};
