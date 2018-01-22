#ifndef TS_NET_CONNECTIONFACTORY_H_
#define TS_NET_CONNECTIONFACTORY_H_

#include <string.h>
#include <stdlib.h>
#include <memory>
#include <map>
#include <set>
#include "core/tigerso.h"
#include "core/BaseClass.h"
#include "net/EventsLoop.h"
#include "net/Connection.h"

namespace tigerso {

class ConnectionFactory: public nocopyable {

public:
    ConnectionFactory(std::shared_ptr<EventsLoop>&);
    std::shared_ptr<Connection>& createConnection(const ConnectionType);    

    int countAliveConnection();
    int registerChannel(Socket&);
    void recycle(Connection*);
    ~ConnectionFactory();

private:
    std::map<unsigned int, std::shared_ptr<Connection>> connections_;
    std::map<ConnectionType, std::set<unsigned int>> gc_;
    std::shared_ptr<EventsLoop> epollptr_;
    ConnectionType type_;
    static unsigned int allocID();
    static unsigned int idbase_;
   
};

} //namespace tigerso

#endif
