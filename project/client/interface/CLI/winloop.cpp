#include "../../include/CLI/winloop.hpp"
#include <iostream>
using namespace std;

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