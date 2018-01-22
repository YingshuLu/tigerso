#include <stdio.h>
#include <string>
#include <memory>
#include <functional>
#include "net/Socket.h"
#include "net/SocketUtil.h"
#include "net/EventsLoop.h"
#include "net/Connection.h"
#include "net/ConnectionFactory.h"
#include "net/Acceptor.h"
#include "core/Logging.h"

namespace tigerso {

Acceptor::Acceptor(const std::string& ipaddr, const int& port, const ConnectionType& type)
: ipaddr_(ipaddr), port_(port) {
    listenSockptr_ = std::unique_ptr<Socket>(new Socket);   
    type_ = type;
    if(listenOn() != 0) { exit(EXIT_FAILURE); }
}

int Acceptor::addIntoFactory(std::shared_ptr<ConnectionFactory>& connfact) {
    connfactptr_ = connfact;
    connfactptr_->registerChannel(*listenSockptr_);
    listenSockptr_->setEventHandle(std::bind(&Acceptor::acceptHandle, shared_from_this(), std::placeholders::_1),SOCKET_EVENT_READ);
    listenSockptr_->setEventHandle(std::bind(&Acceptor::errorHandle, shared_from_this(), std::placeholders::_1), SOCKET_EVENT_ERROR);
    listenSockptr_->setEventHandle(std::bind(&Acceptor::errorHandle, shared_from_this(), std::placeholders::_1), SOCKET_EVENT_RDHUP);
    listenSockptr_->enableReadEvent();
    return 0;
}

int Acceptor::postEvent() {
    acceptWaitn_ = (service_.connectionLimit/8 - (service_.connectionLimit - connfactptr_->countAliveConnection()));
    if(acceptWaitn_ > 0) {
        acceptWaitn_--;
        return 0;
    }
    listenSockptr_->enableReadEvent();
    return 0;
}

std::shared_ptr<Connection> Acceptor::generateConnectionTo(const std::string& ipaddr, const int& port) {
    auto connptr = connfactptr_->createConnection(shared_from_this());
    auto serverptr = connptr->getSocketPtr();

    char buf[16] = {0};
    snprintf(buf, sizeof(buf), "%d", port);
    int ret = SocketUtil::Connect(*serverptr, ipaddr, buf);
    if(ret < 0) { serverptr->close(); }
    else {
        serverptr->setNIO(true);
        registerChannel(*serverptr);
        serverptr->setEventHandle(std::bind(&Connection::writeHandleEnter, &(*connptr), std::placeholders::_1), SOCKET_EVENT_WRITE);
        serverptr->setEventHandle(std::bind(&Connection::errorHandleEnter, &(*connptr), std::placeholders::_1), SOCKET_EVENT_ERROR);
        serverptr->setEventHandle(std::bind(&Connection::rdhupHandleEnter, &(*connptr), std::placeholders::_1), SOCKET_EVENT_RDHUP);
        serverptr->setEventHandle(std::bind(&Connection::timeoutHandleEnter, &(*connptr), std::placeholders::_1), SOCKET_EVENT_TIMEOUT);
        serverptr->enableWriteEvent();
    }
    return connptr;
}

int Acceptor::rdhupHandle(Socket& _l) {
    return errorHandle(_l);
}

int Acceptor::errorHandle(Socket& _l) {
    INFO_LOG("listen socket [%d] error: %d, %s", listenSockptr_->getSocket(), errno, strerror(errno));
    listenSockptr_->close();
    //rebuid to auto fix
    if(listenOn() != 0) { exit(EXIT_FAILURE); }
    addIntoFactory(connfactptr_);
    return 0;
}

int Acceptor::acceptHandle(Socket& _l) {

    {
        int ret = 0;
        std::shared_ptr<Connection> connptr = nullptr;
        std::shared_ptr<Socket> clientptr = nullptr;
        while(true) {
            connptr = nullptr;
            clientptr = nullptr;

            connptr = connfactptr_->createConnection(shared_from_this());
            clientptr = connptr->getSocketPtr();

            ret = SocketUtil::Accept(*listenSockptr_, *clientptr);
            if(ret != 0) {
                connptr->recycle();
                if (-1 == ret) {
                    return errorHandle(*listenSockptr_); 
                }
                return EVENT_CALLBACK_CONTINUE;
            }

            DBG_LOG(" ----- connection id [%u], socket [%d] accepted", connptr->getID(), clientptr->getSocket());
            registerChannel(*clientptr);
            clientptr->setEventHandle(std::bind(&Connection::readHandleEnter, &(*connptr), std::placeholders::_1), SOCKET_EVENT_READ);
            clientptr->setEventHandle(std::bind(&Connection::errorHandleEnter, &(*connptr), std::placeholders::_1), SOCKET_EVENT_ERROR);
            clientptr->setEventHandle(std::bind(&Connection::rdhupHandleEnter, &(*connptr), std::placeholders::_1), SOCKET_EVENT_RDHUP);
            clientptr->setEventHandle(std::bind(&Connection::timeoutHandleEnter, &(*connptr), std::placeholders::_1), SOCKET_EVENT_TIMEOUT);
            clientptr->enableReadEvent();
        }
    }

    return EVENT_CALLBACK_CONTINUE;
}

ConnectionType Acceptor::getType() {
    return type_;
}

int Acceptor::registerChannel(Socket& sock) {
    return connfactptr_->registerChannel(sock);
}

void Acceptor::recycle(Connection* connptr) {
    return connfactptr_->recycle(connptr);
}

int Acceptor::initService(const ServiceContext& service) {
    service_ = service;
    return 0;
}

Acceptor::~Acceptor() {}

int Acceptor::listenOn() {
    char portstr[64] = {0};
    snprintf(portstr, sizeof(portstr), "%d", port_);
    int ret = SocketUtil::CreateListenSocket(ipaddr_, portstr, true, *listenSockptr_);
    if(ret != 0) {
        INFO_LOG("Error: can not listen on %s:%d, exit now!", ipaddr_.c_str(), port_);
    }
    return ret;
}

} //namespace tigerso
