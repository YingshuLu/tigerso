#ifndef TS_HTTP_HTTPCONNECTION_H_
#define TS_HTTP_HTTPCONNECTION_H_

#include "net/Connection.h"
#include "net/Acceptor.h"
#include "net/Channel.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "http/HttpParser.h"

namespace tigerso {

typedef enum {
    TCP_INIT,
    HTTPS_HANDSHAKE,
    HTTPS_HANDSHAKE_DONE,
    RECV_REQUEST,
    RECV_REQUEST_DONE,
    SEND_RESPONSE,
    SEND_RESPONSE_DONE,
    HTTPS_CLOSE,
    HTTPS_CLOSE_DONE,
    TCP_CLOSE
} HttpState;

class HttpConnection: public Connection {

public:
    HttpConnection(std::shared_ptr<Acceptor>, const IDType);   
    int readHandle(Socket&);
    int writeHandle(Socket&);
    int errorHandle(Socket&);
    int rdhupHandle(Socket&);
    int timeoutHandle(Socket&);

public:
    int closeHandle(Socket&);

protected:
    virtual void recycle();
    virtual void doGet(HttpRequest&, HttpResponse&);
    virtual void doPost(HttpRequest&, HttpResponse&) {}

private:
    int SSLHandShake(Socket&);
    void switchRequest();
    void resetResources();

private:
    HttpState state_ = TCP_INIT;
    HttpRequest request_;
    HttpResponse response_;
    HttpParser parser_;
};

} //namespace tigerso

#endif
