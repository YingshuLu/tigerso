#ifndef TS_HTTP_HTTPPROXY_H_
#define TS_HTTP_HTTPPROXY_H_

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "core/BaseClass.h"
#include "net/Socket.h"
#include "net/SocketUti.h"
#include "net/Buffer.h"
#include "net/EventsLoop.h"
#include "http/HttpMessage.h"

namespace tigerso::http {

class HttpProxy {
public:
    typedef net::SocketPtr SocketPtr;
    typedef net::EventFunc EventFunc;
    HttpProxy(const SocketPtr& sockptr, EventsLoop& loop);

private:

    SocketPtr clientPtr_;
    EventsLoop& loop_;
    std::map<std::string, SocketPtr> hostsServerPtrs_;
    HttpParser parser_;
    std::vector<SocketPtr> needCloseSocketPtrs_;
    
    void destroy();

    int errorEvent(SocketPtr& sockptr);

    int recvEvent(SocketPtr& sockptr);
    
    int writeEvent(SocketPtr& sockptr);

    bool needClose(SocketPtr& sockptr);

    SocketPtr findServerPtr(const string& host);

    void insertHostSockPtr(std::string& host, SocketPtr& serverPtr);

    void setReadCallback(SocketPtr& sockptr, const EventFunc& func);

    void setWriteCallback(SocketPtr& sockptr, const EventFunc& func);

    void setErrorCallback(SocketPtr& sockptr, const EventFunc& func);

    void enableReadEvent(SocketPtr& sockptr); 

    void disableReadEvent(SocketPtr& sockptr); 

    void enableWriteEvent(SocketPtr& sockptr);

    void disableWriteEvent(SocketPtr& sockptr);
};

}
#endif
