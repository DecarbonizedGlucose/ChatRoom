#pragma once

#include <functional>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <map>

class TerminalInput {
public:
    using func = std::function<void()>;

    TerminalInput();
    ~TerminalInput();

    void start();
    void stop();

    void set_func(char ch, func cb);                  // 设置回车时的回调函数
    void set_enter_callback(std::function<void(const std::string&)> cb);
    void set_enable_display(bool enable); // true: 文字输入模式 false: 键盘命令模式

    void clear_func(char ch);
    void clear_key_func();
    void clear_buffer();
    void clear_enter_func();
    void clear_all_func();

    void print_message(const std::string& msg);       // 输出外部消息，不干扰输入
    void wait();
    
    std::string get_buffer() const;
    bool buffer_empty() const;

private:
    std::thread input_thread;                         // 单开一个线程
    std::mutex mtx;
    std::string input_buffer;
    std::map<char, func> key_cb;                      // 特定按键功能
    bool enable_display = false;                         // 是否启用回车额外功能
    std::function<void(const std::string&)> enter_cb; // 回车额外的回调
    std::atomic<bool> running;

    void redraw_input_line();                         // 重新绘制输入行
    void input_loop();                                // 输入循环，处理输入和回车
};
