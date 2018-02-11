#include <string.h>
#include <stdlib.h>
#include <string>
#include <memory>
#include <functional>
#include "core/tigerso.h"
#include "net/SocketUtil.h"
#include "http/HttpProxyConnection.h"
#include "core/Logging.h"

namespace tigerso {

#define m_client (sockptr_)
#define m_server (serverptr_)
#define PEER_SOCKETPTR(sock) (sock == *m_client? sockptr_ : serverptr_)
#define IS_CLIENT_SOCKET(sock) (sock == *m_client)
#define IS_SERVER_SOCKET(sock) (sock == *m_server)
#define BIND_EVENTHANDLE(xxx) (std::bind(&xxx, this, std::placeholders::_1))
#define SOCKET_ROLE_STRING(sock) (IS_CLIENT_SOCKET(sock)? "client": "server")

static const char* _proxy_state_strings[] = {
    "PROXY_INIT",
    "CLIENT_RECV_REQUEST",
    "CLIENT_RECV_REQUEST_DONE",
    "CLIENT_SEND_200OK",
    "CLIENT_SEND_200OK_DONE",
    "SERVER_HANDSHAKE",
    "SERVER_HANDSHAKE_DONE",
    "CLIENT_HANDSHAKE",
    "CLIENT_HANDSHAKE_DONE",
    "TCP_TUNNEL",
    "SERVER_SEND_REQUEST",
    "SERVER_SEND_REQUEST_DONE",
    "SERVER_RECV_RESPONSE",
    "SERVER_RECV_RESPONSE_DONE",
    "CLIENT_SEND_RESPONSE",
    "CLIENT_SEND_RESPONSE_DONE",
    "CLIENT_CLOSE",
    "CLIENT_CLOSE_DONE",
    "SERVER_CLOSE",
    "SERVER_CLOSE_DONE"
};

typedef enum {
    SERVER_CONNECT_UINIT,
    SERVER_CONNECT_ERROR,
    SERVER_CONNECT_ING,
    SERVER_CONNECT_DONE
} ServerConnectState;

HttpProxyConnection::HttpProxyConnection(std::shared_ptr<Acceptor> connfactptr, const IDType id): Connection(connfactptr, id) {
    serverptr_ = std::shared_ptr<Socket>(new Socket);

}

int HttpProxyConnection::switchBuffers() {
    auto ibufptr = m_client->getInBufferPtr();
    m_server->setOutBufferPtr(ibufptr);
    ibufptr = m_server->getInBufferPtr();
    m_client->setOutBufferPtr(ibufptr);
    return 0;
}

bool HttpProxyConnection::isNeedTunnelMessage(HttpMessage& message) {
    
    int content_length = message.getContentLength();
    if(content_length < 0) { 
        content_length = 0; 
        if(message.isChunked()) {
            auto bptr = message.getBody();
            content_length = bptr->size();
        }
    }

    return false;
}

bool  HttpProxyConnection::isNeedTunnelSSL(const std::string& url) {
    return false;
}


int HttpProxyConnection::getServerConnectState() {
    if(m_server->exist()) {
        return SERVER_CONNECT_DONE;
    }

    //server connect is not established, guess server connect state
    //has called resovler, now only has error / connecting state
    if(request_.isHeaderCompleted()) {
        int ret = resolver_.getResolveState();
        switch(ret) {
            case DNS_ERR:
                return SERVER_CONNECT_ERROR;

            case DNS_ON_PROCESS:
                return SERVER_CONNECT_ING;
        }
    }

    return SERVER_CONNECT_UINIT;
}

int HttpProxyConnection::readHandle(Socket& sock) {
    return RWEventHandle(sock);
}

int HttpProxyConnection::writeHandle(Socket& sock) {
    return RWEventHandle(sock);
}

int HttpProxyConnection::errorHandle(Socket& sock) {
    return IS_CLIENT_SOCKET(sock)? forceCloseClient() : forceCloseServer();
}

int HttpProxyConnection::rdhupHandle(Socket& sock) {
    return IS_CLIENT_SOCKET(sock)? forceCloseClient() : forceCloseServer();
}

int HttpProxyConnection::timeoutHandle(Socket& sock) {
    return IS_CLIENT_SOCKET(sock)? forceCloseClient() : forceCloseServer();
}

int HttpProxyConnection::closeHandle(Socket& sock) {
    int ret = sock.close();
    if(ret != 0) {
        sock.disableEvent(SOCKET_EVENT_WRITE | SOCKET_EVENT_READ);
        int event = SOCKET_EVENT_NONE;
        if(sock.SSLWantReadMore()) { event = SOCKET_EVENT_READ; }
        else if(sock.SSLWantWriteMore()) { event = SOCKET_EVENT_WRITE; }
        else { sock.tcpClose(); }

        if(event) {
            sock.setEventHandle(BIND_EVENTHANDLE(HttpProxyConnection::closeHandle), event);
            sock.enableEvent(event);
            return EVENT_CALLBACK_CONTINUE;
        }
    }

    //client and server socket close success
    if(!m_client->exist() && !m_server->exist()) {
        recycle(); 
        return EVENT_CALLBACK_BREAK;
    }

    //client closed, close server socket
    if(IS_CLIENT_SOCKET(sock)) {
        return closeHandle(*m_server);
    }

    //server closed, client handle
    else if(IS_SERVER_SOCKET(sock)) {
        auto obufptr = m_client->getOutBufferPtr();
        if(!obufptr->isSendDone()) {
            state_ = CLIENT_SEND_RESPONSE; 
            m_client->enableWriteEvent();
        }
        else {
            m_client->enableReadEvent();
        }
    }
    return EVENT_CALLBACK_CONTINUE;
}

int HttpProxyConnection::requestHeaderCompleteHandle(HttpMessage& request) {

    //Connect to server
    std::string host = request.getHost();
    std::string port = request.getHostPort();
    int ret = 0;
    
    std::string method = request_.getMethod();
    if(!strcasecmp(method.c_str(), "head")) {
        response_.shouldNoBody();
    }

    if(host_ != host || port_ != port || !m_server->exist()) { 
        forceCloseServer();
        response_.clear();
        host_ = host;
        port_ = port;
        
        resolver_.setCallback(std::bind(&HttpProxyConnection::serverConnectHandle, this, std::placeholders::_1, std::placeholders::_2));
        ret = resolver_.resolve(host);
        
        //failed to connect server
        if(DNS_ERR == ret) {
            return TIGERSO_IO_ERROR;
        }

        //immediately connected
        else if(DNS_OK == ret) {

        }

        // on process
        else {

        }
    }

    return TIGERSO_IO_OK;
}

int HttpProxyConnection::serverConnectHandle(const char* ipaddr, time_t ttl) {

    int ret = 0;
    if(ipaddr == NULL || strlen(ipaddr) == 0) { ret = -1; }
    else {
        std::string port = request_.getHostPort();
        if (SocketUtil::Connect(*m_server, ipaddr, port) < 0) {
            INFO_LOG("failed to connect to server: %s:%s, %s", ipaddr, port.c_str(), strerror(errno));
            ret = -1;
        }
        else {
            ret = 0;
            m_server->setNIO(true);
            registerChannel(*m_server);
        
            m_server->setEventHandle(BIND_EVENTHANDLE(HttpProxyConnection::writeHandle), SOCKET_EVENT_WRITE);
            m_server->setEventHandle(BIND_EVENTHANDLE(HttpProxyConnection::readHandle), SOCKET_EVENT_READ);
            m_server->setEventHandle(BIND_EVENTHANDLE(HttpProxyConnection::errorHandle), SOCKET_EVENT_ERROR);
            m_server->setEventHandle(BIND_EVENTHANDLE(HttpProxyConnection::rdhupHandle), SOCKET_EVENT_RDHUP);
            m_server->setEventHandle(BIND_EVENTHANDLE(HttpProxyConnection::timeoutHandle), SOCKET_EVENT_TIMEOUT);
        }
    }

    //client is waiting for server connect, need to tiger next action
    if(state_ == CLIENT_RECV_REQUEST_DONE) {
        //server connected
        if(0 == ret) {
            RWEventHandle(*m_server);
        }
        //server failed to connect
        else {
            HttpHelper::prepare503Response(notification_);
            m_client->getOutBufferPtr()->attachHttpMessage(&notification_);
            m_client->enableWriteEvent();
        }
    }
    return ret;
}

/*SSL TUNNEL BEGIN*/

int HttpProxyConnection::SSLTunnelInit() {
    m_client->enableReadEvent();
    if(m_client->getOutBufferPtr()->getReadableBytes() > 0) { m_client->enableWriteEvent(); }
    else { m_client->disableWriteEvent(); }

    m_server->enableReadEvent();
    if(m_server->getOutBufferPtr()->getReadableBytes() > 0) { m_server->enableWriteEvent(); }
    else { m_server->disableWriteEvent(); }

    m_client->setEventHandle(BIND_EVENTHANDLE(HttpProxyConnection::SSLTunnelReadHandle), SOCKET_EVENT_READ);
    m_client->setEventHandle(BIND_EVENTHANDLE(HttpProxyConnection::SSLTunnelWriteHandle), SOCKET_EVENT_WRITE);
    m_server->setEventHandle(BIND_EVENTHANDLE(HttpProxyConnection::SSLTunnelReadHandle), SOCKET_EVENT_READ);
    m_server->setEventHandle(BIND_EVENTHANDLE(HttpProxyConnection::SSLTunnelWriteHandle), SOCKET_EVENT_WRITE);
    return EVENT_CALLBACK_CONTINUE; 
}

int HttpProxyConnection::SSLTunnelReadHandle(Socket& sock) {
    int recvn = sock.recvNIO();
    if(recvn < 0) {
        INFO_LOG("%s [%d] recv failed, errno: %d, %s", SOCKET_ROLE_STRING(sock), sock.getSocket(), errno, strerror(errno));
        return closeHandle(sock);
    }

    DBG_LOG("%s [%d] tunnel read: %d bytes",SOCKET_ROLE_STRING(sock), sock.getSocket(), recvn);
    SocketPtr peer = PEER_SOCKETPTR(sock);
    peer->enableWriteEvent();
    return EVENT_CALLBACK_CONTINUE;
}

int HttpProxyConnection::SSLTunnelWriteHandle(Socket& sock) {
    auto obufptr = sock.getOutBufferPtr();
    if(!obufptr->getReadableBytes()) { return EVENT_CALLBACK_CONTINUE; }

    int sendn = sock.sendNIO();
    if(sendn < 0) {
        INFO_LOG("%s [%d] send failed, errno: %d : %s",SOCKET_ROLE_STRING(sock), sock.getSocket(), errno, strerror(errno));
        return closeHandle(sock);
    }

    DBG_LOG("%s [%d] tunnel write %d bytes",SOCKET_ROLE_STRING(sock), sock.getSocket(), sendn);
    if(obufptr->getReadableBytes() > 0) { return EVENT_CALLBACK_CONTINUE; }

    sock.disableWriteEvent();
    sock.enableReadEvent();
    return EVENT_CALLBACK_CONTINUE;
}
/*SSL TUNNEL END*/

int HttpProxyConnection::RWEventHandle(Socket& sock) {

    SocketPtr sockptr = IS_CLIENT_SOCKET(sock)? m_client : m_server;
    int ret = 0;
    int recvn = 0;
    int sendn = 0;
    auto ibufptr = sock.getInBufferPtr();
    auto obufptr = sock.getOutBufferPtr();
    while(1) {
        INFO_LOG("Http proxy state: %s", _proxy_state_strings[state_]);
        switch(state_) {
            case PROXY_INIT: {
                switchBuffers();
                request_.setHeaderCompletedHandle(BIND_EVENTHANDLE(HttpProxyConnection::requestHeaderCompleteHandle));
                predictNextState();
                break;
            }

            case CLIENT_RECV_REQUEST:
                request_.tunnelBody();
                response_.tunnelBody();

            case SERVER_RECV_RESPONSE: {
                //switch to socket
                sockptr = (state_ == CLIENT_RECV_REQUEST)? m_client : m_server;
                ibufptr = sockptr->getInBufferPtr();
                obufptr = sockptr->getOutBufferPtr();
                HttpMessage* message;
                if(IS_CLIENT_SOCKET(*sockptr)) { message = &request_; }
                else { message = &response_; }

                sockptr->enableReadEvent();
                sockptr->disableWriteEvent();

                recvn = sockptr->recvNIO();
                if(recvn < 0) {
                    INFO_LOG("%s [%d] read error", SOCKET_ROLE_STRING(*sockptr), sockptr->getSocket());
                    return closeHandle(*sockptr);
                }
                else {
                    DBG_LOG("%s [%d] read %d bytes", SOCKET_ROLE_STRING(*sockptr), sockptr->getSocket(), recvn);
                    int event = SOCKET_EVENT_NONE;
                    if(!sockptr->SSLNoError()) {
                        sockptr->disableReadEvent();
                        sockptr->disableWriteEvent();
                        if(sockptr->SSLWantReadMore()) { event = SOCKET_EVENT_READ; }
                        else if(sockptr->SSLWantWriteMore()) { event = SOCKET_EVENT_WRITE; }
                        else { return closeHandle(*sockptr); }
                        sockptr->enableEvent(event);
                        if(recvn == 0) {
                            if(event & SOCKET_EVENT_WRITE) {
                                return EVENT_CALLBACK_CONTINUE;
                            }
                        }
                    }
                    if(recvn == 0) {
                        if(!message->needMoreData()) {
                            predictNextState();
                            sockptr->disableReadEvent();
                            sockptr->disableWriteEvent();
                            break;
                        }

                        //need read more data
                        return EVENT_CALLBACK_CONTINUE;
                    }
                }

                const char* buffer = ibufptr->getReadPtr() + ibufptr->getReadableBytes() - recvn;
                int parsedn = 0;
                ret = message->parse(buffer, recvn, &parsedn);

                //Error http message
                if(ret == HP_PARSE_ERROR) {
                        if(IS_CLIENT_SOCKET(*sockptr)) {
                            INFO_LOG("Error: Bad Request");
                            HttpHelper::prepare400Response(notification_);
                            obufptr->attachHttpMessage(&notification_);
                            forceCloseServer();
                            predictNextState();
                            m_client->disableReadEvent();
                            m_client->disableWriteEvent();
                            break;
                        }
                        else {
                            INFO_LOG("Error: Bad Response");
                            forceCloseProxy();
                        }
                        return EVENT_CALLBACK_BREAK;
                }

                //need more data
                else if ( ret == HP_PARSE_NEED_MOREDATA) {
                    //partly tunnel 
                    if(IS_CLIENT_SOCKET(*sockptr)) {
                        switch(getServerConnectState()) {

                            //server connect error, send notification
                            case SERVER_CONNECT_ERROR: {
                                HttpHelper::prepare503Response(notification_);
                                obufptr->attachHttpMessage(&notification_);
                                break;
                            }

                            //wait server connect, continue read more data
                            case SERVER_CONNECT_UINIT:
                            case SERVER_CONNECT_ING: {
                                return EVENT_CALLBACK_CONTINUE;
                            }

                            //server connected
                            case SERVER_CONNECT_DONE: {
                                //client request, decide if tunnel this request
                                std::string method = request_.getMethod();
                                if(!strcasecmp(method.c_str(), "connect")) {
                                    return EVENT_CALLBACK_CONTINUE;
                                }
                                return EVENT_CALLBACK_CONTINUE;
                            }
                        }
                    }
                    else {
                        //server response decide if tunnel this response
                        return EVENT_CALLBACK_CONTINUE;
                    }

                }

                //body recieved completely
                else {
                    if(IS_CLIENT_SOCKET(*sockptr)) {
                        switch(getServerConnectState()) {
                            //server connect error
                            case SERVER_CONNECT_ERROR: {
                                HttpHelper::prepare503Response(notification_);
                                obufptr->attachHttpMessage(&notification_);
                                break;
                            }

                            //wait server connect
                            case SERVER_CONNECT_ING: {
                                INFO_LOG("client [%d] wait server to establish connection", m_client->getSocket());
                                predictNextState();
                                sockptr->disableReadEvent();
                                sockptr->disableWriteEvent();
                                return EVENT_CALLBACK_BREAK; 
                            }

                            //server connected
                            case SERVER_CONNECT_DONE: {

                                break;
                            }
                        }
                    }
                }
                
                predictNextState();
                sockptr->disableReadEvent();
                sockptr->disableWriteEvent();
                break;
            }
            case CLIENT_RECV_REQUEST_DONE:
            case SERVER_RECV_RESPONSE_DONE: {
                predictNextState();
                if(state_ == CLIENT_SEND_200OK) {
                    INFO_LOG("prepare 200 OK response");
                    HttpHelper::prepare200Response(notification_);
                    m_client->getOutBufferPtr()->attachHttpMessage(&notification_);
                }
                break;
            }

            case CLIENT_SEND_200OK:
            case CLIENT_SEND_RESPONSE:
            case SERVER_SEND_REQUEST: {
                sockptr = (state_ == SERVER_SEND_REQUEST)? m_server : m_client;
                ibufptr = sockptr->getInBufferPtr();
                obufptr = sockptr->getOutBufferPtr();
                sockptr->enableWriteEvent();
                sockptr->disableReadEvent();

                DBG_LOG("[Before] %s [%d] buffer size: %d",SOCKET_ROLE_STRING(*sockptr), sockptr->getSocket(), obufptr->getReadableBytes());
                sendn = sockptr->sendNIO();
                if(sendn < 0) {
                    INFO_LOG("%s [%d] write error",SOCKET_ROLE_STRING(*sockptr), sockptr->getSocket());
                    return closeHandle(*sockptr);
                }
            
                DBG_LOG("%s [%d] write %d bytes, left buffer size: %d",SOCKET_ROLE_STRING(*sockptr), sockptr->getSocket(), sendn, obufptr->getReadableBytes());
                if(!obufptr->isSendDone()) {
                    if(!sockptr->SSLNoError()) {
                        sockptr->disableReadEvent();
                        sockptr->disableWriteEvent();
                        int event = SOCKET_EVENT_NONE;
                        if(sockptr->SSLWantReadMore()) { event = SOCKET_EVENT_READ; }
                        else if(sockptr->SSLWantWriteMore()) { event = SOCKET_EVENT_WRITE; }
                        else { return closeHandle(*sockptr); }
                        sockptr->enableEvent(event);
                    }
                    return EVENT_CALLBACK_CONTINUE;
                }

                DBG_LOG("%s [%d] write all data completed", SOCKET_ROLE_STRING(*sockptr), sockptr->getSocket());
                //write all data, decide next action, update state
                predictNextState();
                sockptr->disableReadEvent();
                sockptr->disableWriteEvent();
                break;
            }

            case CLIENT_SEND_200OK_DONE:
            case CLIENT_SEND_RESPONSE_DONE: {
                request_.clear();
                response_.clear();
            }
            case SERVER_SEND_REQUEST_DONE: {
                predictNextState();
                if(SERVER_HANDSHAKE == state_) {
                    if(m_server->prepareSSLContext() != 0 || m_client->prepareSSLContext() != 0) {
                        INFO_LOG("failed to init SSLContext");
                        forceCloseProxy();
                        return EVENT_CALLBACK_BREAK;
                    }
                }
                break;
            }

            case TCP_TUNNEL: {
                SSLTunnelInit();
                break;
            }

            case SERVER_HANDSHAKE: {
                m_server->enableWriteEvent();
                m_server->disableReadEvent();

                ret = serverSSLHandShake();
                if(TIGERSO_IO_RECALL == ret) {
                    return EVENT_CALLBACK_CONTINUE;
                }
                else if(TIGERSO_IO_ERROR == ret) { return forceCloseProxy(); }

                ret = resignCertificate();
                if(ret != 0) { return forceCloseProxy(); }

                predictNextState();
                m_server->disableReadEvent();
                m_server->disableWriteEvent();
                break;
            }

            case SERVER_HANDSHAKE_DONE: {
                predictNextState();
                //set up resigned certificate and private key
                if(m_client->getSSLContextPtr()->resetupCertKey() < 0) {
                    forceCloseProxy();
                    return EVENT_CALLBACK_BREAK;
                }
                break;
            }

            case CLIENT_HANDSHAKE: {
                m_client->enableReadEvent();
                m_client->disableWriteEvent();

                ret = clientSSLHandShake();
                if(TIGERSO_IO_ERROR == ret) {
                    INFO_LOG("client SSL handshake failed");
                    forceCloseProxy();
                    return EVENT_CALLBACK_BREAK;
                }
                else if(TIGERSO_IO_RECALL == ret) {
                    return EVENT_CALLBACK_CONTINUE;
                }

                predictNextState();

                m_client->disableReadEvent();
                m_client->disableWriteEvent();
                break;
            }

            case CLIENT_HANDSHAKE_DONE: {
                predictNextState();
                break;
            }

            default: {
                INFO_LOG("invalid state: %d", state_);
                forceCloseProxy();
                return EVENT_CALLBACK_BREAK;
            }
                
        } //switch(state_)

    } // while(1)
                
    return EVENT_CALLBACK_BREAK;
}

int HttpProxyConnection::forceCloseProxy() {
    m_client->tcpClose();
    forceCloseServer();
    return 0;
}

int HttpProxyConnection::forceCloseServer() {
    m_server->tcpClose();
    if(!m_client->exist()) { recycle(); }
    return 0;
}

int HttpProxyConnection::forceCloseClient() {
    return forceCloseProxy();
}

int HttpProxyConnection::predictNextState() {
    switch(state_) {
        case PROXY_INIT:
            state_ = CLIENT_RECV_REQUEST;
            m_client->enableReadEvent();
            break;

        case CLIENT_RECV_REQUEST:
            if(!request_.needMoreData()) {
                state_ = CLIENT_RECV_REQUEST_DONE;
            }
            break;

        case CLIENT_RECV_REQUEST_DONE:
            //m_server connection should be established
            if(m_server->exist()) {
                state_ = strcasecmp(request_.getMethod().c_str(), "connect") == 0? CLIENT_SEND_200OK : SERVER_SEND_REQUEST;
            }
            else {
                //send error notification
                state_ = CLIENT_SEND_RESPONSE;
            }
            break;

        case CLIENT_SEND_200OK:
            state_ = CLIENT_SEND_200OK_DONE;
            break;

        case CLIENT_SEND_200OK_DONE:
            state_ = isNeedTunnelSSL(request_.getHost())? TCP_TUNNEL: SERVER_HANDSHAKE;
            break;

        case SERVER_HANDSHAKE:
            state_ = SERVER_HANDSHAKE_DONE;
            break;

        case SERVER_HANDSHAKE_DONE:
            state_ = CLIENT_HANDSHAKE;
            break;

        case CLIENT_HANDSHAKE:
            state_ = CLIENT_HANDSHAKE_DONE;
            break;

        case CLIENT_HANDSHAKE_DONE:
            state_ = CLIENT_RECV_REQUEST;
            break;
        
        case SERVER_SEND_REQUEST:
            state_ = SERVER_SEND_REQUEST_DONE;
            break;

        case SERVER_SEND_REQUEST_DONE:
            state_ = SERVER_RECV_RESPONSE;
            break;

        case SERVER_RECV_RESPONSE:
            state_ = SERVER_RECV_RESPONSE_DONE;
            break;

        case SERVER_RECV_RESPONSE_DONE:
            state_ = CLIENT_SEND_RESPONSE;
            break;

        case CLIENT_SEND_RESPONSE:
            state_ = CLIENT_SEND_RESPONSE_DONE;
            break;

        case CLIENT_SEND_RESPONSE_DONE:
            state_ = CLIENT_RECV_REQUEST;
            break;

        case CLIENT_CLOSE:
            state_ = CLIENT_CLOSE_DONE;
            break;

        case CLIENT_CLOSE_DONE:
            state_ = m_server->exist()? SERVER_CLOSE : PROXY_INIT;
            break;
            
        case SERVER_CLOSE:
            state_ = SERVER_CLOSE_DONE;
            break;

        case SERVER_CLOSE_DONE:
            state_ = m_client->exist()? CLIENT_SEND_RESPONSE: PROXY_INIT;
            break;

        default:
            break;

    }
    return state_;
}

int HttpProxyConnection::serverSSLHandShake() {
    int ret = 0;
    int event = SOCKET_EVENT_NONE;
    if(state_ == SERVER_HANDSHAKE) {
        ret = m_server->SSLConnect();
        switch(ret) {
            case SCTX_IO_OK:
                DBG_LOG("[Https] Server SSL handshake success");
                return TIGERSO_IO_OK;

            case SCTX_IO_RECALL:
                m_server->disableReadEvent();
                m_server->disableWriteEvent();
                if(m_server->SSLWantReadMore()) { event = SOCKET_EVENT_READ; }
                else if(m_server->SSLWantWriteMore()) { event = SOCKET_EVENT_WRITE; }
                m_server->enableEvent(event);
                return TIGERSO_IO_RECALL;

            case SCTX_IO_ERROR:
                INFO_LOG("[Https] Server SSL handshake failed");
                return TIGERSO_IO_ERROR;
        }
    }
    return TIGERSO_IO_ERROR;
}

int HttpProxyConnection::clientSSLHandShake() {
    int ret = 0;
    int event = SOCKET_EVENT_NONE;
    if(state_ == CLIENT_HANDSHAKE) {
        ret = m_client->SSLAccept();
        switch(ret) {
            case SCTX_IO_OK:
                DBG_LOG("[Https] Client SSL handshake success");
                return TIGERSO_IO_OK;

            case SCTX_IO_RECALL:
                m_client->disableReadEvent();
                m_client->disableWriteEvent();
                if(m_client->SSLWantReadMore()) { event = SOCKET_EVENT_READ; }
                else if(m_client->SSLWantWriteMore()) { event = SOCKET_EVENT_WRITE; }
                m_client->enableEvent(event);
                return TIGERSO_IO_RECALL;

            case SCTX_IO_ERROR:
                INFO_LOG("[Https] Client SSL handshake failed");
                return TIGERSO_IO_ERROR;
        }
    }
    return TIGERSO_IO_ERROR;
}

int HttpProxyConnection::resignCertificate() {
    return SSLHelper::resignContextCert(*(m_server->getSSLContextPtr()), *(m_client->getSSLContextPtr()));
}

void HttpProxyConnection::recycle() {
    host_.clear();
    port_.clear();
    request_.clear();
    response_.clear();
    m_client->reset();
    m_server->reset();
    resolver_.reset();
    state_ = PROXY_INIT;
    Connection::recycle();
}

}//namespace tigerso
