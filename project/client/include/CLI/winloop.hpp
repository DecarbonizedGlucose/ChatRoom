#pragma once

#include "output.hpp"
#include <mutex>
#include <condition_variable>

class TerminalInput;
class CommManager;

void show_start_menu();
void show_register_menu();
void show_login_menu();

void show_main_menu();
void show_message_list();
void show_contacts_list();
void show_about_info();

class StartWin {
public:
    StartWin(TerminalInput* input, CommManager* comm);
    ~StartWin();
    void init();

    void main_loop();
    void login_loop();
    void regi_loop();

    std::mutex mtx;
    std::condition_variable cv;

private:
    bool running = false;
    TerminalInput* input = nullptr;
    CommManager* comm;
    int select;
};

class MainWin {
public:
    MainWin(TerminalInput* input, CommManager* comm);
    ~MainWin();
    void init();

    void main_loop();
    void message_list_loop();
    void contacts_list_loop();
    void about_info_loop();

    std::mutex mtx;
    std::condition_variable cv;

private:
    bool running = false;
    TerminalInput* input = nullptr;
    CommManager* comm;
    int select;
};

