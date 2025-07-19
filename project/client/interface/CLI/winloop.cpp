#include "../../include/CLI/winloop.hpp"
#include "../../include/CLI/output.hpp"
#include "../../include/CLI/TerminalInput.hpp"
#include "../../../global/include/logging.hpp"
#include "../../include/chat/main/CommManager.hpp"
#include "../../../global/include/action.hpp"
#include <iostream>

/* ---------- 暗黑逻辑学 ---------- */

StartWin::StartWin(TerminalInput* input, CommManager* comm)
    : running(true), input(input), comm(comm) {}

StartWin::~StartWin() {
    running = false;
    input->stop();
}

void StartWin::init() {
    input->start(); // 开始监听输入
}

void StartWin::main_loop() {
    while (running) {
        select = 0;
        input->clear_key_func();
        input->set_func('1', [&]{
            std::lock_guard<std::mutex> lock(mtx);
            select = 1;
            cv.notify_one();
        });
        input->set_func('2', [&]{
            std::lock_guard<std::mutex> lock(mtx);
            select = 2;
            cv.notify_one();
        });
        input->set_func(27, [&]{ // ESC
            std::lock_guard<std::mutex> lock(mtx);
            select = -1;
            cv.notify_one();
        });
        sclear();
        show_start_menu();
        // 等待用户输入
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&] { return select != 0; });
        switch (select) {
            case 1: {
                login_loop();
                break;
            }
            case 2: {
                regi_loop();
                break;
            }
            case -1: {
                break;
            }
            default: {
                // 你是？
                // 理论上这块不应该被触发
                log_error("Unexpected selection: {}", select);
                continue;
            }
        }
        if (!running) { break; }
    }
}

void StartWin::login_loop() {
    while (running) {
        select = 0;
        input->clear_key_func();
        input->set_enable_display(true);
        input->set_enter_callback([&](const std::string& email) { // 第一次回调
            // 配合handler::login_handler
        });
        input->set_func(27, [&]{ // ESC
            std::lock_guard<std::mutex> lock(mtx);
            select = -1;
            cv.notify_one();
        });
        sclear();
        std::cout << style("Please enter your Email:", {ansi::BOLD, ansi::FG_GRAY}) << std::endl;
        print_input_sign();
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [&]{ return !input->buffer_empty(); }); // 等待输入
        }
        input->set_enter_callback([&](const std::string& password) { // 第二次回调
            // 配合handler::login_handler
        });
        input->clear_buffer();
        std::cout << style("Please enter your Password:", {ansi::BOLD, ansi::FG_GRAY}) << std::endl;
        print_input_sign();
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [&]{ return !input->buffer_empty(); }); // 等待输入
        }
        // 服务器传来登录结果
        // ...
        // ...
        if (!running || select == -1) { break; }
    }
}

void StartWin::regi_loop() {
    while (running) {
        select = 0;
        input->clear_key_func();
        input->set_enable_display(true);
        input->set_enter_callback([&](const std::string& email) { // 第一次回调
            // 配合handler::regi_handler
            // 发送验证码
            comm->handle_send_command(Action::Get_Veri_Code, email, {});
        });
        input->set_func(27, [&]{ // ESC
            std::lock_guard<std::mutex> lock(mtx);
            select = -1;
            cv.notify_one();
        });
        sclear();
        std::cout << style("Please enter your Email:", {ansi::BOLD, ansi::FG_GRAY}) << std::endl;
        print_input_sign();
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [&]{ return select!=0 || !input->buffer_empty(); }); // 等待输入
        }
        if (select == -1) { break; }
        // 提取邮箱
        std::string email = input->get_buffer();
        std::cout << style("Please enter the verification code sent to your Email:", {ansi::BOLD, ansi::FG_GRAY}) << std::endl;
        print_input_sign();
        input->clear_buffer();
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [&]{ return select!=0 || !input->buffer_empty(); }); // 等待输入
        }
        if (select == -1) { break; }
        // 提取验证码
        std::string code = input->get_buffer();
        // 提交注册信息
        comm->handle_send_command(Action::Register, email, {code});
        // 服务器回应
        // ...
        if (!running || select == -1) { break; }
    }
}

/* ---------- 纯UI部分 ---------- */

void show_start_menu() {
    std::cout << "$ ----- Welcome to the Chat Room ----- $" << std::endl << std::endl;
    std::cout << "               " << style("[1]", {ansi::FG_BRIGHT_BLUE}) << " Log in" << std::endl << std::endl;
    std::cout << "              " << style("[2]", {ansi::FG_BRIGHT_BLUE}) << " Register" << std::endl;
}

void show_register_menu() {
    //std::cout << "Please register to continue." << std::endl;
}

void show_login_menu() {
    //std::cout << "Please log in to your account." << std::endl;
}

void show_main_menu() {
    // 需要展示未读消息数
}

void show_message_list() {
    //std::cout << "Message List:" << std::endl;
}

void show_contacts_list() {
    //std::cout << "Contacts List:" << std::endl;
}

void show_about_info() {
    std::cout << "Chat Room Client" << std::endl;
    std::cout << "Author : decglu" << std::endl;
    std::cout << "Repo : https://github.com/DecarbonizedGlucose/ChatRoom.git" << std::endl;
}