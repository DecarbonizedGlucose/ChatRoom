#pragma once

#include "output.hpp"
#include <mutex>
#include <condition_variable>

class TerminalInput;

void show_start_menu();
void show_register_menu();
void show_login_menu();

void show_main_menu();
void show_message_list();
void show_contacts_list();
void show_about_info();

class StartWin {
public:
    StartWin();

    void main_loop();
    void regi_loop();

    std::mutex mtx;
    std::condition_variable cv;

private:
    bool running = false;
    TerminalInput* input = nullptr;
    void clear();
    int select;
};

class MainWin {

};

