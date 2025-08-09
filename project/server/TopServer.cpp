#include "include/TopServer.hpp"
#include "include/TcpServer.hpp"
#include "include/dispatcher.hpp"
#include "../global/include/threadpool.hpp"
#include "../global/include/logging.hpp"
#include "include/connection_manager.hpp"
#include "include/sfile_manager.hpp"
#include <nlohmann/json.hpp>
#include <fstream>

namespace mysql_config {
    std::string host;
    std::string user;
    std::string password;
    std::string dbname;
    unsigned port = 3306U;
    void config(std::string file) {
        std::ifstream ifs(file);
        if (!ifs.is_open()) {
            throw std::runtime_error("Failed to open config file");
        }

        std::string content;
        content.assign((std::istreambuf_iterator<char>(ifs)),
                       (std::istreambuf_iterator<char>()));
        ifs.close();

        auto j = nlohmann::json::parse(content);
        host = j["host"].get<std::string>();
        user = j["user"].get<std::string>();
        password = j["password"].get<std::string>();
        dbname = j["dbname"].get<std::string>();
        port = j["port"].get<unsigned int>();
    }
}

TopServer::TopServer() {
    pool = new thread_pool(20);
    redis = new RedisController();
    mysql = new MySQLController(
        mysql_config::host,
        mysql_config::user,
        mysql_config::password,
        mysql_config::dbname,
        mysql_config::port
    );
    disp = new Dispatcher(redis, mysql);
    message_server = new TcpServer(0);
    command_server = new TcpServer(1);
    data_server = new TcpServer(2);
    disp->add_server(message_server, 0);
    disp->add_server(command_server, 1);
    disp->add_server(data_server, 2);
}

TopServer::~TopServer() {
    log_info("TopServer destructor called, cleaning up resources");
    delete message_server;
    delete command_server;
    delete data_server;
    delete disp;
    delete redis;
    delete pool;
}

bool TopServer::launch() {
    pool->init();
    if (!mysql->connect()) {
        log_error("Failed to connect to MySQL");
        return false;
    }

    // 设置 SFileManager 的线程池
    disp->file_manager->set_thread_pool(pool);

    message_server->init(pool, disp);
    command_server->init(pool, disp);
    data_server->init(pool, disp);
    pool->submit([this]() {
        message_server->start();
    });
    log_info("Message server submited to thread pool");
    pool->submit([this]() {
        command_server->start();
    });
    log_info("Command server submited to thread pool");
    pool->submit([this]() {
        data_server->start();
    });
    log_info("Data server submited to thread pool");
    return true;
}

void TopServer::stop() {
    log_info("Stopping TopServer and all associated servers");
    if (message_server) {
        message_server->stop();
    }
    if (command_server) {
        command_server->stop();
    }
    if (data_server) {
        data_server->stop();
    }
    pool->shutdown();
    mysql->disconnect();
    log_info("All servers stopped");
}

