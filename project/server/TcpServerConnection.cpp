#include "include/TcpServerConnection.hpp"
#include "../io/include/Socket.hpp"
#include "../io/include/reactor.hpp"
#include "../global/abstract/datatypes.hpp"
//#include "../global/include/safe_queue.hpp"
#include "include/dispatcher.hpp"
#include <chrono>

TcpServerConnection::TcpServerConnection(reactor* reactor_ptr, Dispatcher* disp)
    : reactor_ptr(reactor_ptr), dispatcher(disp) {}

void TcpServerConnection::set_send_type(DataType type) {
    to_send_type = type;
}

TcpServerConnection::~TcpServerConnection() {
    if (read_event) {
        delete read_event;
    }
    if (write_event) {
        delete write_event;
    }
    if (socket) {
        delete socket;
    }
}