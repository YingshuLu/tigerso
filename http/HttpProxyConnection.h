#ifndef TS_HTTP_HTTPPROXYCONNECTION_H_
#define TS_HTTP_HTTPPROXYCONNECTION_H_

#include "dns/DNSResolver.h"
#include "net/Connection.h"
#include "net/ConnectionFactory.h"
#include "net/Acceptor.h"
#include "net/Channel.h"
#include "http/HttpMessage.h"
#include "http/HttpParser.h"

namespace tigerso {

typedef enum {
    STATE_RESET,
    SERVER_HANDSHAKE,
    SERVER_HANDSHAKE_DONE,
    CLIENT_HANDSHAKE,
    CLIENT_HANDSHAKE_DONE,
    CLIENT_RECV_REQUEST,
    CLIENT_RECV_REQUEST_DONE,
    SERVER_SEND_REQUEST,
    SERVER_SEND_REQUEST_DONE,
    SERVER_RECV_RESPONSE,
    SERVER_RECV_RESPONSE_DONE,
    CLIENT_SEND_RESPONSE,
    CLIENT_SEND_RESPONSE_DONE,
    CLIENT_CLOSE,
    CLIENT_CLOSE_DONE,
    SERVER_CLOSE,
    SERVER_CLOSE_DONE
} HttpProxyState;

class HttpProxyConnection: public Connection {

public:
    HttpProxyConnection(std::shared_ptr<Acceptor> connfactptr, const IDType id);

public:
    int readHandle(Socket&);
    int writeHandle(Socket&);
    int errorHandle(Socket&);
    int rdhupHandle(Socket&);
    int timeoutHandle(Socket&);

public:
    int closeHandle(Socket&);

protected:
    ConnectionType getType() { return HTTP_PROXY_CONNECTION; }

private:
    
    int serverSSLHandShake();
    int clientSSLHandShake();
    
    int clientReadHandle(Socket&);
    int serverReadHandle(Socket&);
    
    int clientRequestHandle(const int);
    int serverResponseHandle();

    int clientWriteFinalData(Socket&);
    int serverWriteFinalData(Socket&);
    int switchBuffers();
    void reset();
    void resetState();

private:
    std::shared_ptr<Socket> serverptr_;
    std::shared_ptr<Socket> udpptr_;
    DNSResolver dns_;
    HttpProxyState state_ = STATE_RESET;
    HttpRequest request_;
    HttpResponse response_;
    HttpParser parser_;

};

} //namespace tigerso

#endif
