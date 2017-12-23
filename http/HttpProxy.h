#ifndef TS_HTTP_HTTPPROXY_H_
#define TS_HTTP_HTTPPROXY_H_
#include <iostream>
#include <typeinfo>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <memory>
#include <functional>
#include "stdio.h"
#include "net/Socket.h"
#include "net/SocketUtil.h"
#include "net/EventsLoop.h"
#include "net/Channel.h"
#include "http/HttpMessage.h"
#include "http/HttpParser.h"
#include "util/FileTypeDetector.h"
#include "dns/DNSResolver.h"
#include "core/Logging.h"

namespace tigerso {

typedef unsigned long IDTYPE;  

class HttpProxyConnection {
typedef std::function<int(HttpProxyConnection&)> LOOP_CALLBACK ;
#define _clientSocket (*csockptr)
#define _serverSocket (*ssockptr)
#define _dnsSocket    (*dsockptr)

#define BIND_EVENTHANDLE(xxx) (std::bind(&xxx, this, std::placeholders::_1))
struct HttpTransaction {
    HttpRequest request;
    HttpResponse response;
};

public:
    HttpProxyConnection();
    const IDTYPE& getID();
    ~HttpProxyConnection();

/*Socket close handle*/
public:
    /*safe close will close socket after all epoll event handled, it is a delay*/
    int clientSafeClose(Socket& client); 
    int serverSafeClose(Socket& server);
    int clientCloseHandle(Socket& client);
    int serverCloseHandle(Socket& server);
    void closeProxyConnection();
    void forceCloseProxyConnection();
    int socketNullHandle(Socket& sock);
public:
    int clientRDHUPHandle(Socket& client);
    int serverRDHUPHandle(Socket& server);
    int clientErrorHandle(Socket& client);
    int serverErrorHandle(Socket& server);
    int clientFinalWriteHandle(Socket& client);

public:
    int clientFirstReadHandle(Socket& client);      
    int serverConnectTo(const char* ip, time_t ttl);
    int serverFirstWriteHandle(Socket& server);
    int serverReadHandle(Socket& server);
    int clientOnlyWriteHandle(Socket& client);    
    int clientWriteHandle(Socket& client);
    int clientReadHandle(Socket& client);
    int serverOnlyWriteHandle(Socket& server);
    int serverWriteHandle(Socket& server);
    int clientTunnelWriteHandle(Socket& client);
    int clientTunnelReadHandle(Socket& client);
    int serverTunnelWriteHandle(Socket& server);    
    int serverTunnelReadHandle(Socket& server);
    void setSocketRegisterFunc(EventFunc f);
    void setEraseFunc(LOOP_CALLBACK cb);
private:
    //Exchange In/Out Buffer between client and server
    void transferProxyBuffer();
    void serverDecideSkipBody();

public:
    void setEventsLoopPtr(EventsLoop* loop);

private:
    //Reset resources and unregister from proxy loop
    void finalize();
    static IDTYPE uuid();

private:
    bool socketEnableReadEvent(Socket& sock);
    bool socketEnableWriteEvent(Socket& sock);    
    bool socketDisableReadEvent(Socket& sock);
    bool socketDisableWriteEvent(Socket& sock);
    bool socketSetEventHandle(Socket& sock, EventFunc func, unsigned short flag);    
    bool isClientAlive() { return _clientSocket.exist(); }
    bool isServerAlive() { return _serverSocket.exist(); }

private:
    HttpProxyConnection(const HttpProxyConnection&);
    HttpProxyConnection& operator=(const HttpProxyConnection&);

private:
    LOOP_CALLBACK erase_func_ = nullptr;
    EventFunc register_func_ = nullptr;

private:
    HttpTransaction ctransaction_;
    HttpTransaction stransaction_;

private:
    const IDTYPE  ID_;
    HttpParser cparser_;
    HttpParser sparser_;
    bool keepalive_ = true;
    DNSResolver resolver_;
    EventsLoop* elooptr_ = nullptr;
    static IDTYPE base_;

private:
    bool client2close_ = false;
    bool server2close_ = false;

public:
    //HttpProxyConnection life circle depend on client socket, client socket never to be changed
    const SocketPtr csockptr;
    const SocketPtr ssockptr;
    const SocketPtr dsockptr;
};


typedef std::shared_ptr<HttpProxyConnection> HTTPPROXYCONNECTIONPTR;
/*
 * HttpProxyPool start with events loop, and save HttpProxyConnection with shared pointer
 * the HttpProxyConnection instance can be reused on the loop to avoid frequently memory alloc or free.
 *
 */
class HttpProxyLoop {   

public:
    HttpProxyLoop(const std::string& ipaddr, const std::string& port);
    //Parent call
    int initListenConnection();
    //Child call
    int startLoop();
    /*Get current client connection number*/
    int countHttpConnections();
    
public:
     int masterErrorHandle(Socket& master);     
     int masterAfterHandle(Socket& master);
     int acceptHttpClientConnection(Socket& master);
     // callback for HttpProxyConnection to relaese itself
     int discardThisHttpProxyConnection(HttpProxyConnection& hpl);
     int registerSocket(Socket& sock);   

private:
    int eraseHttpProxyConnection(HttpProxyConnection& hpl);
    HTTPPROXYCONNECTIONPTR makeHttpProxyConnection();
    void registerFunctionsForHttpProxyConnection(HttpProxyConnection& hpc);
    int insertHttpProxyConnectionPtr(HTTPPROXYCONNECTIONPTR& hcptr);
    // start here,  listening
    int listenHttpClientConnection();
    void clear();

private:
    HttpProxyLoop(const HttpProxyLoop&);
    HttpProxyLoop& operator=(const HttpProxyLoop&);

private:
    //strong link with shared pointer, keep connection alive.
    std::map<IDTYPE, HTTPPROXYCONNECTIONPTR> connections_; 
    //mark as discard, recycle it
    std::set<IDTYPE> discardIDs_;

private:
    const SocketPtr msockptr_;
    EventsLoop eloop_;
    const std::string ipaddr_;
    const std::string port_;
};

} // namespace tigerso::http
#endif
