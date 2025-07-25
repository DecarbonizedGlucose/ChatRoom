#pragma once

class TerminalInput;
class CommManager;
class ChatMessage;

#include <memory>
#include <string>
#include <functional>
#include <mutex>
#include "output.hpp"

class CommManager;
class thread_pool;
class CommandRequest;

// 页面状态枚举
enum class UIPage {
    Start,
    Login,
    Register,
    Main,
    Message,
    Contacts,
    My,
    Exit
};

class WinLoop {
public:
    WinLoop(CommManager* comm, thread_pool* pool);
    ~WinLoop();

    void run();
    void stop();
    void dispatch_cmd(const CommandRequest& cmd);

    bool running = false;

private:
    // 处理用户输入(仅选择)
    void handle_start_input();
    void handle_main_input();
    // 各个页面
    void start_loop();
    void login_loop();
    void register_loop();
    void main_init(); // 进入主界面前的初始化
    void main_loop();
    void message_loop();
    void contacts_loop();
    void my_loop();
    // 无界面功能
    void log_out();
    // 页面跳转
    void switch_to(UIPage page);
    // 工具函数
    void sclear();
    void pause();

    UIPage current_page;

    CommManager* comm = nullptr; // 通信管理器
    thread_pool* pool = nullptr; // 线程池

    std::mutex output_mutex;
};

// 页面渲染
void draw_start(std::mutex& mtx);
void draw_login(std::mutex& mtx, int idx);
void draw_register(std::mutex& mtx, int idx);
void draw_main(std::mutex& mtx, const std::string& user_ID);