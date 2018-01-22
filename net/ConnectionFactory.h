#ifndef TS_NET_CONNECTIONFACTORY_H_
#define TS_NET_CONNECTIONFACTORY_H_

#include <string.h>
#include <stdlib.h>
#include <memory>
#include <map>
#include <set>
#include <vector>
#include <queue>
#include "core/tigerso.h"
#include "core/BaseClass.h"
#include "net/Socket.h"
#include "net/EventsLoop.h"
#include "net/Connection.h"

namespace tigerso {

class Acceptor;
class ConnectionFactory: public std::enable_shared_from_this<ConnectionFactory> {

public:
    ConnectionFactory();
    ConnectionFactory(std::shared_ptr<EventsLoop>&);
    std::shared_ptr<Connection>& createConnection(std::shared_ptr<Acceptor>&&);    
    int start();
    int stop();
    int countAliveConnection();
    int registerChannel(Socket&);
    void recycle(Connection*);
    ~ConnectionFactory();

private:
    std::map<IDType, std::shared_ptr<Connection>> connections_;
    std::map<ConnectionType, std::queue<IDType>> gc_;
    std::shared_ptr<EventsLoop> epollptr_ = nullptr;
    static IDType allocID();
    static IDType idbase_;
   
};

} //namespace tigerso

#endif
