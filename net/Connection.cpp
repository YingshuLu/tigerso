#include <string.h>
#include <stdlib.h>
#include <memory>
#include "core/tigerso.h"
#include "core/BaseClass.h"
#include "net/Socket.h"
#include "net/Acceptor.h"

namespace tigerso {

Connection::Connection(std::shared_ptr<Acceptor> acptptr, const IDType id): acptptr_ (acptptr), id_(id) {
    sockptr_ = std::shared_ptr<Socket>(new Socket);
}

IDType Connection::getID() { return id_; }

int Connection::readHandleEnter(Socket& _l) { return readHandle(_l); }
int Connection::writeHandleEnter(Socket& _l) { return writeHandle(_l); }
int Connection::errorHandleEnter(Socket& _l) { return errorHandle(_l); }
int Connection::rdhupHandleEnter(Socket& _l) { return rdhupHandle(_l); }
int Connection::timeoutHandleEnter(Socket& _l) { return timeoutHandle(_l); }
std::shared_ptr<Socket>& Connection::getSocketPtr() { return sockptr_; }
Connection::~Connection() {}

ConnectionType Connection::getType() { return acptptr_->getType(); }  

int Connection::registerChannel(Socket& sock) {
    return acptptr_->registerChannel(sock); 
}

void Connection::recycle() {
    if(acptptr_ != nullptr) {
        acptptr_->recycle(this);
    }
    return;
}

void Connection::linkToAcceptor(std::shared_ptr<Acceptor> acptptr) { acptptr_ = acptptr; }




} //namespace tigerso
