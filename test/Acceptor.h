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
    
class Socket;
class ConnectionFactory;
class ShmMutex;
class EventsLoop;

class Acceptor: public nocopyable {

public:
    Acceptor(const std::string&, const int&, const ConnectionType&);
    static int setConnectionLimitation(const int);
    int addIntoLoop(std::shared_ptr<ConnectionFactory>&);
    int rdhupHandle(Socket&);
    int errorHandle(Socket&); 
    int acceptHandle(Socket&);
    ~Acceptor();

private:
    int listenOn();

private:
    std::shared_ptr<ConnectionFactory> connfactptr_ = nullptr;
    std::unique_ptr<Socket> listenSockptr_ = nullptr;
    ShmMutex mutex_;
    ConnectionType type_;
    const std::string ipaddr_;
    const int port_;

};

} //namespace tigerso

#endif
