#include "../../include/CLI/winloop.hpp"
#include <iostream>
#include "../../include/CLI/TerminalInput.hpp"
using namespace std;

/* ---------- 暗黑逻辑学 ---------- */

StartWin::StartWin() : running(true) {
    input = new TerminalInput();
    input->start(); // 开始监听输入
}

void StartWin::main_loop() {
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
    while (running) {
        clear();
        show_start_menu();
        // 等待用户输入
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&] { return select != 0; });
        switch (select) {
            case 1: {
                break;
            }
            case 2: {
                break;
            }
            case -1: {
                break;
            }
            default: {
                // 你是？
                continue;
            }
        }
    }
}

void StartWin::regi_loop() {

}

void StartWin::clear() {
    system("clear");
}


/* ---------- 纯输出部分 ---------- */

void show_start_menu() {
    cout << "$ ----- Welcome to the Chat Room ----- $" << endl << endl;
    cout << "               " << style("[1]", {ansi::FG_BRIGHT_BLUE}) << " Log in" << endl << endl;
    cout << "              " << style("[2]", {ansi::FG_BRIGHT_BLUE}) << " Register" << endl;
}

void show_register_menu() {
    //cout << "Please register to continue." << endl;
}

void show_login_menu() {
    //cout << "Please log in to your account." << endl;
}

void show_main_menu() {
    // 需要展示未读消息数
}

void show_message_list() {
    //cout << "Message List:" << endl;
}

void show_contacts_list() {
    //cout << "Contacts List:" << endl;
}

void show_about_info() {
    cout << "Chat Room Client" << endl;
    cout << "Author : DecGlu" << endl;
    cout << "Repo : https://github.com/DecarbonizedGlucose/ChatRoom.git" << endl;
}