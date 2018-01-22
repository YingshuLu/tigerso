#include <string.h>
#include <stdlib.h>
#include <memory>
#include "core/tigerso.h"
#include "core/BaseClass.h"
#include "net/Socket.h"
#include "net/ConnectionFactory.h"

namespace tigerso {

Connection::Connection(std::shared_ptr<ConnectionFactory>& connfactptr, const unsigned int id) {
    connfactptr_ = connfactptr;
    clientptr_ = std::shared_ptr<Socket>(new Socket);
    id_ = id;
}

unsigned int Connection::getID() { return id_; }
int Connection::readHandleEnter(Socket& _l) { return readHandle(_l); }
int Connection::writeHandleEnter(Socket& _l) { return writeHandle(_l); }
int Connection::errorHandleEnter(Socket& _l) { return errorHandle(_l); }
int Connection::rdhupHandleEnter(Socket& _l) { return rdhupHandle(_l); }
int Connection::timeoutHandleEnter(Socket& _l) { return timeoutHandle(_l); }

viod Connection::recyle() {
    if(connfactptr_ != nullptr) {
        connfactptr_->recyle(this);
    }
    return;
}

std::shared_ptr<Socket>& Connection::getClientSocket() { return clientptr_; }
Connection::~Connection() {}


} //namespace tigerso
