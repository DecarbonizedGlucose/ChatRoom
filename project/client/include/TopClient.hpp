#pragma once

class TerminalInput;
class TcpClient;
class thread_pool;
class StartWin;
class MainWin;
class CommManager;

class TopClient {
public:
    std::unique_ptr<TcpClient> message_client;
    std::unique_ptr<TcpClient> command_client;
    std::unique_ptr<TcpClient> data_client;
    std::unique_ptr<thread_pool> pool;
    TerminalInput* input;
    CommManager* comm_manager;

    TopClient();
    void launch();
    void stop();

private:
    bool running = false;
    StartWin* start_win;
    MainWin* main_win;
};
