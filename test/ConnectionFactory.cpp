#include <string.h>
#include <stdlib.h>
#include <memory>
#include "core/tigerso.h"
#include "core/BaseClass.h"
#include "net/Connection.h"
#include "net/EventsLoop.h"

namespace tigerso {

ConnectionFactory::ConnectionFactory(std::shared_ptr<EventsLoop>& eloop): epollptr_(eloop) {}

std::shared_ptr<Connection>& ConnectionFactory::createConnection(const ConnectionType type) {
    // reuse the connection (same type)
    auto recycleSet = gc_[type];
    auto iter = recycleSet.begin();
    if(iter != recycle.end()) { 
        unsigned int id = *iter;
        recycleSet.erase(id);
        return connections_[id];
    }

    std::shared_ptr<Connection> connptr;
    switch (type) {
        HTTP_CONNECTION:
            connptr = std::shared_ptr<HttpProxyConnection> (new HttpProxyConnection(this, allocID()));
            break;

        HTTP_CONNECTION:
            connptr = std::shared_ptr<HttpConnection>(new HttpConnection(this, allocID()));
            break;

        TCP_CONNECTION:
        default:
            connptr = nullptr;
    }

    connections_.[connptr->getID()] = connptr;
    return connptr;
}

int ConnectionFactory::countAliveConnection() {
    int all = connections_.size();
    for(int i = 0; i < TCP_UNKNOWN; i++) { 
        all -= gc_[i].size();
    }
    return all;
}

int ConnectionFactory::registerChannel(Socket& sock) {
    return epollptr_->registerChannel(sock);
}

void ConnectionFactory::recycle(Connection* conn) {
    gc_[conn.getType()].insert(conn->getID());
    return;
}

ConnectionFactory::~ConnectionFactory() {}

unsigned int ConnectionFactory::allocID() {
    if(idbase_ >= ~(0)) { idbase_ = 0; }
    return ++idbase_;
}

unsigned int ConnectionFactory::idbase_ = 0;

} //namespace tigerso

