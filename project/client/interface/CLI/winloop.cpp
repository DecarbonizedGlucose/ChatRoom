#include "../../include/CLI/winloop.hpp"
#include "../../include/CLI/output.hpp"
#include "../../../global/include/logging.hpp"
#include "../../include/chat/main/CommManager.hpp"
#include "../../../global/include/action.hpp"
#include <iostream>

WinLoop::WinLoop(CommManager* comm) : current_page(UIPage::Start), comm(comm) {}

WinLoop::~WinLoop() {
}

void WinLoop::run() {
    running = true;
    while (running) { // 状态机比那循环嵌套好看多了
        switch (current_page) {
            case UIPage::Start:
                start_loop();
                break;
            case UIPage::Login:
                login_loop();
                break;
            case UIPage::Register:
                register_loop();
                break;
            case UIPage::Main:
                main_loop();
                break;
            case UIPage::Message:
                message_loop();
                break;
            case UIPage::Contacts:
                contacts_loop();
                break;
            case UIPage::About:
                about_loop();
                break;
            case UIPage::Exit:
                running = false;
                break;
            default:
                log_error("Unknown page state: {}", static_cast<int>(current_page));
        }
    }
}

void WinLoop::stop() {
    running = false;
}

/* ---------- 页面处理 ---------- */

void WinLoop::start_loop() {
    sclear();
    draw_start();
    handle_start_input();
}

void WinLoop::login_loop() {
    while (1) {
        sclear();
        draw_login(1);
        std::string email, password;
        std::getline(std::cin, email);
        if (email.empty()) { // 返回
            switch_to(UIPage::Start);
            return;
        }
        draw_login(2);
        std::getline(std::cin, password);
        if (email.empty() || password.empty()) {
            std::cout << "Email or password cannot be empty." << std::endl;
            pause();
            continue;
        }
        // 传给服务器
        comm->handle_send_command(Action::Sign_In, email, {password});
        // 读取服务器响应
        CommandRequest resp = comm->handle_receive_command();
        if (resp.action() == static_cast<int>(Action::Accept)) {
            std::cout << "登录成功！" << std::endl;
            switch_to(UIPage::Main);
            return;
        } else { // Refused
            std::cout << "登录失败" << resp.args(0) << std::endl;
            pause();
            continue;
        }
    }
}

void WinLoop::register_loop() {
    while (1) {
        sclear();
        std::string email, veri_code, username, password;
        draw_register(1);
        std::getline(std::cin, email);
        if (email.empty()) { // 返回
            switch_to(UIPage::Start);
            return;
        }
        // 发送验证码请求
        comm->handle_send_command(Action::Get_Veri_Code, email, {});
        log_info("Sent veri code request");
        // 读取服务器响应
        CommandRequest resp = comm->handle_receive_command();
        log_info("Received veri code response");
        int action_ = resp.action();
        if (action_ != static_cast<int>(Action::Accept)) {
            std::cout << "邮箱已存在。" << std::endl;
            pause();
            continue;
        }
        draw_register(2);
        std::getline(std::cin, veri_code);
        if (veri_code.empty()) {
            std::cout << "验证码不能为空。" << std::endl;
            pause();
            continue;
        }
        // 发送验证码
        comm->handle_send_command(Action::Authentication, email, {veri_code});
        // 读取服务器响应
        CommandRequest auth_resp = comm->handle_receive_command();
        if (auth_resp.action() != static_cast<int>(Action::Accept)) {
            std::cout << "身份验证失败：" << auth_resp.args(0) << std::endl;
            pause();
            continue;
        }
        draw_register(3);
        std::getline(std::cin, username);
        if (username.empty()) {
            std::cout << "用户名不能为空。" << std::endl;
            pause();
            continue;
        }
        draw_register(4);
        std::getline(std::cin, password);
        if (password.empty()) {
            std::cout << "密码不能为空。" << std::endl;
            pause();
            continue;
        }
        // 发送注册请求
        comm->handle_send_command(Action::Register, email, {username, password});
        log_info("Sent registration request");
        // 读取服务器响应
        CommandRequest reg_resp = comm->handle_receive_command();
        log_info("Received registration response");
        if (reg_resp.action() == static_cast<int>(Action::Accept)) {
            log_info("Successfully registered");
            std::cout << "注册成功！" << std::endl;
            switch_to(UIPage::Main);
            return;
        } else {
            log_info("Registration failed: {}", reg_resp.args(0));
            std::cout << reg_resp.args(0) << std::endl;
            pause();
            continue;
        }
    }
}

void WinLoop::main_loop() {}

void WinLoop::message_loop() {}

void WinLoop::contacts_loop() {}

void WinLoop::about_loop() {

}

/* ---------- 菜单选择 ---------- */

void WinLoop::handle_start_input() {
    std::string input;
    std::getline(std::cin, input);
    if (input == "1") {
        switch_to(UIPage::Login);
    } else if (input == "2") {
        switch_to(UIPage::Register);
    } else if (input == "0") {
        switch_to(UIPage::Exit);
    } else {
        log_error("Invalid input: {}", input);
    }
}

/* ---------- tools ---------- */

void WinLoop::sclear() {
    system("clear"); // 或者 system("cls") 在 Windows 上
}

void WinLoop::pause() {
    std::cout << "按任意键继续...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

void WinLoop::switch_to(UIPage page) {
    current_page = page;
}

/* ---------- 渲染 ---------- */

void draw_start() {
    std::cout << "-$-    欢迎来到聊天室 -$-" << std::endl;
    std::cout << "         [1] 登录" << std::endl;
    std::cout << "         [2] 注册" << std::endl;
    std::cout << "         [0] 退出" << std::endl;
    print_input_sign();
}

void draw_login(int idx) {
    switch (idx) {
        case 1:
            std::cout << "         -$- 登    录 -$-" << std::endl;
            std::cout << "请输入邮箱和密码。" << std::endl;
            std::cout << "（直接回车返回）" << std::endl;
            std::cout << "邮箱";
            print_input_sign();
            break;
        case 2:
            std::cout << "密码";
            print_input_sign();
            break;
    }
}

void draw_register(int idx) {
    switch (idx) {
        case 1:
            std::cout << "          -$- 注    册 -$-" << std::endl;
            std::cout << "请输入邮箱、验证码、用户名和密码。" << std::endl;
            std::cout << "（直接回车返回）" << std::endl;
            std::cout << "邮箱";
            print_input_sign();
            break;
        case 2:
            std::cout << "请注意查收验证码。验证码5分钟内有效。" << std::endl;
            std::cout << "验证码";
            print_input_sign();
            break;
        case 3:
            std::cout << "用户名";
            print_input_sign();
            break;
        case 4:
            std::cout << "密码";
            print_input_sign();
            break;
    }
}

void draw_main() {
}