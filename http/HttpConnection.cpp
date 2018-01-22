#include "net/Connection.h"
#include "net/ConnectionFactory.h"
#include "net/Channel.h"
#include "http/HttpConnection.h"

namespace tigerso {

#define m_sock (*sockptr__)
#define BIND_EVENTHANDLE(xxx) (std::bind(&xxx, this, std::placeholders::_1))

HttpConnection::HttpConnection(std::shared_ptr<Acceptor> acptptr, const IDType id): Connection(acptptr, id) {}
   
int HttpConnection::readHandle(Socket& sock){

    int ret = TIGERSO_IO_OK;
    int recvn = 0;
    int parsedn = 0;
    auto ibufptr = sock.getInBufferPtr();
    auto obufptr = sock.getOutBufferPtr();
    const char* beg  = nullptr;

    while(1) {
        switch(state_) {
            case TCP_INIT:
                if(acptptr_->SSLProtocol()) {
                    sock.prepareSSLContext();
                    state_= HTTPS_HANDSHAKE;
                }
                else { state_ = RECV_REQUEST; }
                break;

            case HTTPS_HANDSHAKE:
                ret = SSLHandShake(sock);
                if(ret != TIGERSO_IO_OK) { return EVENT_CALLBACK_CONTINUE; }
                state_ = HTTPS_HANDSHAKE_DONE;
                break;

            case HTTPS_HANDSHAKE_DONE:
                state_ = RECV_REQUEST;
                break;

            case RECV_REQUEST:
                recvn = sock.recvNIO();
                if(TIGERSO_IO_ERROR == recvn) {
                    if(sock.isSSL()) {
                        sock.disableReadEvent();
                        sock.disableWriteEvent();
                        int event = SOCKET_EVENT_NONE;
                        if(sock.SSLWantReadMore()) { event = SOCKET_EVENT_READ; }
                        else if(sock.SSLWantWriteMore()) { event = SOCKET_EVENT_WRITE; }
                        else { return closeHandle(sock); }
                        sock.setEventHandle(BIND_EVENTHANDLE(HttpConnection::readHandle), event);
                        sock.disableEvent(SOCKET_EVENT_READ);
                        sock.enableEvent(event);
                        return EVENT_CALLBACK_CONTINUE;
                    }
                    return closeHandle(sock);
                }
                else if(0 == recvn) {
                    if(!parser_.needMoreData()) { return closeHandle(sock); }
                    sock.setEventHandle(BIND_EVENTHANDLE(HttpConnection::readHandle), SOCKET_EVENT_READ);
                    sock.disableWriteEvent();
                    sock.enableReadEvent();
                    return EVENT_CALLBACK_CONTINUE;
                }

                DBG_LOG("recv request, %d bytes", recvn);
                beg = ibufptr->getReadPtr() + ibufptr->getReadableBytes() - recvn;
                parsedn = parser_.parse(beg, recvn, request_);
                INFO_LOG("http parser return: %d", parsedn);
        
                //Bad request
                if(parsedn != recvn) {
                    INFO_LOG("Error: failed to parser http request");
                    HttpHelper::prepare400Response(response_);
                    obufptr->attachHttpMessage(&response_);
                    sock.disableReadEvent();
                    sock.setEventHandle(BIND_EVENTHANDLE(HttpConnection::writeHandle), SOCKET_EVENT_WRITE);
                    sock.enableWriteEvent();
                    return EVENT_CALLBACK_BREAK;
                }
        
                if(parser_.needMoreData()) { return EVENT_CALLBACK_CONTINUE; }
                //recv request done
                state_ = RECV_REQUEST_DONE;
                break;

            case RECV_REQUEST_DONE:
                switchRequest();
                DBG_LOG("response:\n%s", response_.toString().c_str());
                obufptr->attachHttpMessage(&response_); 
                sock.setEventHandle(BIND_EVENTHANDLE(HttpConnection::writeHandle), SOCKET_EVENT_WRITE);
                sock.disableReadEvent();
                sock.enableWriteEvent();

                state_ = SEND_RESPONSE;
                return EVENT_CALLBACK_CONTINUE;

        }
    }
    return EVENT_CALLBACK_CONTINUE;
}

int HttpConnection::writeHandle(Socket& sock) {
    int ret = sock.sendNIO();
    if(TIGERSO_IO_ERROR == ret) {
        if(sock.isSSL()) {
            sock.disableReadEvent();
            sock.disableWriteEvent();
            int event = SOCKET_EVENT_NONE;
            if(sock.SSLWantReadMore()) { event = SOCKET_EVENT_READ; }
            else if(sock.SSLWantWriteMore()) { event = SOCKET_EVENT_WRITE; }
            else { return closeHandle(sock); }
            sock.setEventHandle(BIND_EVENTHANDLE(HttpConnection::writeHandle), event);
            sock.enableEvent(event);
            return EVENT_CALLBACK_CONTINUE;
        }
        return closeHandle(sock);
    }

    auto obufptr = sock.getOutBufferPtr();
    if(!obufptr->isSendDone()) {
        return EVENT_CALLBACK_CONTINUE;
    }

    state_ = SEND_RESPONSE_DONE;
    if(response_.keepalive()) {
        resetResources();
        sock.disableWriteEvent();
        sock.setEventHandle(BIND_EVENTHANDLE(HttpConnection::readHandle), SOCKET_EVENT_READ);
        sock.enableReadEvent();
        return EVENT_CALLBACK_CONTINUE;
    }
    
    INFO_LOG("keepalive is FALSE, close this Connection");
    return closeHandle(sock);
}

int HttpConnection::errorHandle(Socket& sock){
        return closeHandle(sock);
    }

int HttpConnection::rdhupHandle(Socket& sock){
        return closeHandle(sock);
    }

int HttpConnection::timeoutHandle(Socket& sock){
        return sock.tcpClose();
}

int HttpConnection::closeHandle(Socket& sock) {
    int ret = sock.close();
    if(ret != 0) {
        if(sock.isSSL()) {
            sock.disableReadEvent();
            sock.disableWriteEvent();
            int event = SOCKET_EVENT_NONE;
            if(sock.SSLWantReadMore()) { event = SOCKET_EVENT_READ; }
            else if(sock.SSLWantWriteMore()) { event = SOCKET_EVENT_WRITE; }
            sock.setEventHandle(BIND_EVENTHANDLE(HttpConnection::closeHandle), event);
            sock.enableEvent(event);
            return EVENT_CALLBACK_CONTINUE;
        }
    }
    //close success
    recycle();
    return EVENT_CALLBACK_BREAK;
}

void HttpConnection::doGet(HttpRequest& request, HttpResponse& response) {

    std::size_t pos = request.getUrl().find_first_of("/");
    std::string filepath = acptptr_->getServiceRoot();
    if(pos != std::string::npos && request.getUrl().substr(pos).size() > 1) {
        filepath.append(request.getUrl().substr(pos));
    }
    else {
        filepath.append("/");
        filepath.append("index.html");
    }

    if(!File(filepath).testExist()) {
        HttpHelper::prepare403Response(response);
        return;
    }

    response.setStatuscode(200);
    bool keepalive = request.keepalive();
    response.setKeepAlive(keepalive);
    response.setBodyFileName(filepath);
    response.setValueByHeader("Server", "tigerso");
    response_.setChunkedTransfer(true);
    
}

int HttpConnection::SSLHandShake(Socket& sock) {
    if(!sock.isSSL()) {
        INFO_LOG("Error: ssl context is not inited");
        return closeHandle(sock);
    }
    
    int ret = sock.SSLAccept();
    int event = SOCKET_EVENT_NONE;

    switch(ret) {
        case SCTX_IO_OK:
            DBG_LOG("SSL handshake success");
            state_ = HTTPS_HANDSHAKE_DONE;
            sock.disableWriteEvent();
            sock.setEventHandle(BIND_EVENTHANDLE(HttpConnection::readHandle), SOCKET_EVENT_READ);
            sock.enableReadEvent();
            return TIGERSO_IO_OK;

        case SCTX_IO_RECALL:
            sock.disableReadEvent();
            sock.disableWriteEvent();
            if (sock.SSLWantReadMore()) { event = SOCKET_EVENT_READ; }
            else if (sock.SSLWantWriteMore()) { event = SOCKET_EVENT_WRITE; }
            sock.setEventHandle(BIND_EVENTHANDLE(HttpConnection::readHandle), event);
            sock.enableEvent(event);
            return TIGERSO_IO_RECALL;

        case SCTX_IO_ERROR:
            INFO_LOG("SSLHandShake failed");
            closeHandle(sock);
            return TIGERSO_IO_ERROR;
    }

    return TIGERSO_IO_ERROR;
}

void HttpConnection::recycle() {
    sockptr__->reset();
    resetResources();
    state_ = TCP_INIT;
    Connection::recycle();
}

void HttpConnection::resetResources() {
   m_sock.getInBufferPtr()->clear();
   m_sock.getOutBufferPtr()->clear();
   request_.clear();
   response_.clear();
   parser_.reset();
   state_ = RECV_REQUEST;
}

void HttpConnection::switchRequest() {
    std::string method = request_.getMethod();
    doGet(request_, response_);
    return;
}

} //namespace tigerso
