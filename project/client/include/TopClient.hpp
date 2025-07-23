#pragma once

// class TerminalInput;
class TcpClient;
class thread_pool;
// class StartWin;
// class MainWin;
class CommManager;
class WinLoop;

class TopClient {
public:
    TcpClient* message_client;
    TcpClient* command_client;
    TcpClient* data_client;
    thread_pool* pool;
    CommManager* comm;
    WinLoop* winloop;

    TopClient();
    ~TopClient();

    void launch();
    void stop();

private:
    bool running = false;
    // StartWin* start_win;
    // MainWin* main_win;
};
