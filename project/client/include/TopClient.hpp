#pragma once

// class TerminalInput;
class TcpClient;
class thread_pool;
// class StartWin;
// class MainWin;
class CommManager;

class TopClient {
public:
    TcpClient* message_client;
    TcpClient* command_client;
    TcpClient* data_client;
    thread_pool* pool;
    // TerminalInput* input;
    // CommManager* comm_manager;

    TopClient();
    void launch();
    void stop();

private:
    bool running = false;
    // StartWin* start_win;
    // MainWin* main_win;
};
