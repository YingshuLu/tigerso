#ifndef TS_NET_ACCEPTOR_H_
#define TS_NET_ACCEPTOR_H_

#include <stdlib.h>
#include <string.h>
#include <string>
#include <memory>
#include "core/tigerso.h"
#include "core/SysUtil.h"
#include "net/ConnectionFactory.h"
#include "net/Socket.h"
#include "net/EventsLoop.h"

namespace tigerso {

struct ServiceContext {
    std::string host; //only for host services, i.e. Http Service 
    std::string rootDir = "./";
    int connectionLimit = 1024;
    bool sslEnabled = false;
};
    
class Acceptor: public std::enable_shared_from_this<Acceptor> {

public:
    Acceptor(const std::string&, const int&, const ConnectionType&);
    int addIntoFactory(std::shared_ptr<ConnectionFactory>&);
    int postEvent();

    std::shared_ptr<Connection> generateConnectionTo(const std::string&, const int&);
    int rdhupHandle(Socket&);
    int errorHandle(Socket&); 
    int acceptHandle(Socket&);
    ~Acceptor();

public:
    int initService(const ServiceContext& service);
    inline bool SSLProtocol() { return service_.sslEnabled; }
    const std::string& getServiceHost() { return service_.host; }
    const std::string& getServiceRoot() { return service_.rootDir;  }

public:
    ConnectionType getType();
    //proxy functions for ConnectionFactory 
    int registerChannel(Socket&);
    void recycle(Connection*);

private:
    int listenOn();

private:
    ServiceContext service_;
    int acceptWaitn_ = 0;
    std::shared_ptr<ConnectionFactory> connfactptr_ = nullptr;
    std::unique_ptr<Socket> listenSockptr_ = nullptr;
    ShmMutex mutex_;
    ConnectionType type_;
    const std::string ipaddr_;
    const int port_;
};

} //namespace tigerso

#endif
