#include <string.h>
#include <stdlib.h>
#include <memory>
#include "core/tigerso.h"
#include "core/BaseClass.h"
#include "net/ConnectionFactory.h"
#include "net/EventsLoop.h"
#include "http/HttpProxyConnection.h"
#include "http/HttpConnection.h"
#include "net/Acceptor.h"
#include "core/Logging.h"

namespace tigerso {

ConnectionFactory::ConnectionFactory() {
    epollptr_ = std::shared_ptr<EventsLoop>(new EventsLoop);
}

ConnectionFactory::ConnectionFactory(std::shared_ptr<EventsLoop>& eloop): epollptr_(eloop) {}

std::shared_ptr<Connection>& ConnectionFactory::createConnection(std::shared_ptr<Acceptor>&& acptptr) {
    ConnectionType type = acptptr->getType();

    // reuse the connection (same type)
    {
        //Note rcle should be referred to gc_[type]
        auto& rcle = gc_[type];
        if(!rcle.empty()) {
            IDType id = rcle.front();
            rcle.pop();
            INFO_LOG("reuse connection id [%d] from gc, now recycle size: %ld", id, rcle.size());
            connections_[id]->linkToAcceptor(acptptr);
            return connections_[id];
        }
    }

    std::shared_ptr<Connection> connptr;
    switch (type) {
        case HTTP_PROXY_CONNECTION:
            connptr = std::shared_ptr<HttpProxyConnection> (new HttpProxyConnection(acptptr, allocID()));
            break;

        case HTTP_CONNECTION:
            connptr = std::shared_ptr<HttpConnection>(new HttpConnection(acptptr, allocID()));
            break;

        case TCP_UNKNOWN:
        default:
            connptr = nullptr;
    }

    assert(connptr != nullptr);
    //INFO_LOG("create new Connection id [%d]", connptr->getID());
    connections_[connptr->getID()] = connptr;
    return connections_[connptr->getID()];
}

int ConnectionFactory::start() {
    epollptr_->loop();
    return 0;
}

int ConnectionFactory::stop() {
    epollptr_->stop();
    return 0;
}

int ConnectionFactory::countAliveConnection() {
    int all = connections_.size();
    for(ConnectionType i = 0; i < TCP_UNKNOWN; i++) { 
        all -= gc_[i].size();
    }
    return all;
}

int ConnectionFactory::registerChannel(Socket& sock) {
    return epollptr_->registerChannel(sock);
}

void ConnectionFactory::recycle(Connection* connptr) {
    INFO_LOG("recycle connection [%d]", connptr->getID());
    gc_[connptr->getType()].push(connptr->getID());
    return;
}

ConnectionFactory::~ConnectionFactory() {}

IDType ConnectionFactory::allocID() {
    if(idbase_ >= ~(0)) { idbase_ = 0; }
    return ++idbase_;
}

IDType ConnectionFactory::idbase_ = 0;

} //namespace tigerso

