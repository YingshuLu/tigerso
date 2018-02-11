#ifndef TS_HTTP_HTTPPROXYCONNECTION_H_
#define TS_HTTP_HTTPPROXYCONNECTION_H_

#include "dns/DNSResolver.h"
#include "net/Connection.h"
#include "net/ConnectionFactory.h"
#include "net/Acceptor.h"
#include "net/Channel.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "http/HttpParser.h"

namespace tigerso {

typedef enum {
    PROXY_INIT,

    CLIENT_RECV_REQUEST,
    CLIENT_RECV_REQUEST_DONE,

    /*Https state start*/
    CLIENT_SEND_200OK,
    CLIENT_SEND_200OK_DONE,

    SERVER_HANDSHAKE,
    SERVER_HANDSHAKE_DONE,

    CLIENT_HANDSHAKE,
    CLIENT_HANDSHAKE_DONE,
    /*Https state done*/

    TCP_TUNNEL,

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
    int requestHeaderCompleteHandle(HttpMessage&);
    int serverConnectHandle(const char*, time_t);
    int SSLTunnelReadHandle(Socket&);
    int SSLTunnelWriteHandle(Socket&);

private:
    int RWEventHandle(Socket&);
    int forceCloseProxy();
    int forceCloseServer();
    int forceCloseClient();

private:
    int predictNextState();
    int resignCertificate();
    int getServerConnectState();
    int serverSSLHandShake();
    int clientSSLHandShake();
    int switchBuffers();
    int SSLTunnelInit();
    //simple policy
    bool isNeedTunnelMessage(HttpMessage&);
    bool isNeedTunnelSSL(const std::string&);

protected:
    void recycle();

private:
    std::shared_ptr<Socket> serverptr_;
    DNSResolver resolver_;
    HttpProxyState state_ = PROXY_INIT;
    HttpRequest request_;
    HttpResponse response_;
    HttpResponse notification_;

private:
    std::string host_;
    std::string port_;
    bool SSLTunnel_ = false;
    bool requestTunnel_ = false;
    bool responseTunnel_ = false;
};

} //namespace tigerso

#endif
