#include <string>
#include <memory>
#include <functional>
#include "net/Socket.h"
#include "net/SocketUtil.h"
#include "net/EventsLoop.h"
#include "net/Connection.h"
#include "net/ConnectionFactory.h"
#include "net/SysUtil.h"
#include "net/Acceptor.h"

namespace tigerso {

int Acceptor::CONNECTION_LIMIT = 1024;

Acceptor::Acceptor(const std::string& ipaddr, const int& port, const ConnectionType& type)
: ipaddr_(ipaddr), port_(port) {
    listenSockptr_ = std::unique_ptr<Socket>(new Socket);   
    type_ = type;
    if(listenOn() != 0) { exit(EXIT_FAILURE); }
}

int Acceptor::setConnectionLimitation(const int num) {
    if (num > 0) {
        if(num > 4096) { CONNECTION_LIMIT = 4096; }
        else { CONNECTION_LIMIT = num; }
    }
    return CONNECTION_LIMIT;
}

int Acceptor::addIntoLoop(std::shared_ptr<ConnectionFactory>& connfact) {
    connfactptr_ = connfact;
    connfactptr_->registerChannel(*listenSockptr_);
    listenSockptr_->setEventHandle(BIND_EVENTHANDLE(Acceptor::acceptHandle),SOCKET_EVENT_READ);
    listenSockptr_->setEventHandle(BIND_EVENTHANDLE(Acceptor::errorHandle), SOCKET_EVENT_ERROR);
    listenSockptr_->setEventHandle(BIND_EVENTHANDLE(Acceptor::rdhupHandle), SOCKET_EVENT_RDHUP);
    listenSockptr_->enableEvent(SOCKET_EVENT_READ);
    return 0;
}

int Acceptor::rdhupHandle(Socket& _l) {
    return errorHandle(_l);
}

int Acceptor::errorHandle(Socket& _l) {
    INFO_LOG("listen socket [%d] error: %d, %s", *listenSockptr_, errno, strerror(errno));
    listenSockptr_.close();
    //rebuid to auto fix
    if(listenOn() != 0) { exit(EXIT_FAILURE); }
    addIntoLoop(connfactptr_);
    return 0;
}

int Acceptor::acceptHandle(Socket& _l) {
    if(CONNECTION_LIMIT - connfactptr_->countAliveConnection() < CONNECTION_LIMIT/16) { return EVENT_CALLBACK_CONTINUE; }

    {
    LockGuard(acceptMutex_);
    std::shared_ptr<Connection> connptr;
    SocketPtr clientptr;
    while(true) {
        connptr = connfactptr_->createConnection(type_);
        clientptr = connptr->getClientSocket();
        if(SocketUtil::Accept(*listenSockptr_, *clientptr) != 0) {
            connptr->recyle();
            if(errno != EAGAIN && errno != EWOULDBLOCK) {
                INFO_LOG("Accept error: %d, %s", errno, strerror(errno));
                return errorHandle(*listenSockptr_); 
            }
            return EVENT_CALLBACK_CONTINUE;
        }
        connfactptr_->registerChannel(*clientptr);
        clientptr->setEventHandle(std::bind(&Connection::readHandleEnter, &(*connptr), std::placeholders::_1), SOCKET_EVENT_READ);
        clientptr->setEventHandle(std::bind(&Connection::errorHandleEnter, &(*connptr), std::placeholders::_1), SOCKET_EVENT_ERROR);
        clientptr->setEventHandle(std::bind(&Connection::rdhupHandleEnter, &(*connptr), std::placeholders::_1), SOCKET_EVENT_RDHUP);
        clientptr->setEventHandle(std::bind(&Connection::timeoutHandleEnter, &(*connptr), std::placeholders::_1), SOCKET_EVENT_TIMEOUT);
    }
    }

    //should nerver be here
    return EVENT_CALLBACK_CONTINUE;
}

Acceptor::~Acceptor() {}

int Acceptor::listenOn() {
    char portstr[64] = {0}
    snprintf(portstr, sizeof(portstr), "%d", port_);
    int ret = SocketUtil::CreateListenSocket(ipaddr_, portstr, true, *listenSockptr_);
    if(ret != 0) {
        INFO_LOG("Error: can not listen on %s:%d, exit now!" ipaddr.c_str(), port);
    }
    return ret;
}

} //namespace tigerso
