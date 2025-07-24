#pragma once

class TerminalInput;
class CommManager;
class ChatMessage;

#include <memory>
#include <string>
#include <functional>
#include "output.hpp"

class CommManager;

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
    WinLoop(CommManager* comm);
    ~WinLoop();

    void run();
    void stop();

private:
    // 处理用户输入(仅选择)
    void handle_start_input();
    void handle_main_input();
    // 各个页面
    void start_loop();
    void login_loop();
    void register_loop();
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
    bool running = false;

    CommManager* comm = nullptr; // 通信管理器
};

// 页面渲染
void draw_start();
void draw_login(int idx);
void draw_register(int idx);
void draw_main();