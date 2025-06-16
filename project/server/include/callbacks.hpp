#ifndef CALLBACKS_HPP
#define CALLBACKS_HPP

#include "../../io/include/reactor.hpp"

MesPtr receive_message(event<>* ev) {
    auto sock = std::dynamic_pointer_cast<ASocket>(ev->get_socket());
    if (sock == nullptr) {
        return nullptr;
    }
    MesPtr message = sock->receive_message();
    if (message == nullptr) {
        return nullptr;
    }
    return message;
}

bool send_message(event<>* ev, const MesPtr& message) {
    auto sock = std::dynamic_pointer_cast<ASocket>(ev->get_socket());
    if (sock == nullptr) {
        return false;
    }
    return sock->send_message(message);
}

ComPtr receive_command(event<>* ev) {
    auto sock = std::dynamic_pointer_cast<ASocket>(ev->get_socket());
    if (sock == nullptr) {
        return nullptr;
    }
    ComPtr command = sock->receive_command();
    if (command == nullptr) {
        return nullptr;
    }
    return command;
}

bool send_command(event<>* ev, const ComPtr command) {
    auto sock = std::dynamic_pointer_cast<ASocket>(ev->get_socket());
    if (sock == nullptr) {
        return false;
    }
    return sock->send_command(command);
}

#endif