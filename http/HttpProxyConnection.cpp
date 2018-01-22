#include "core/tigerso.h"
#include "http/HttpProxyConnection.h"

namespace tigerso {

#define m_client (*sockptr__)
#define m_server (*serverptr_)
#define BIND_EVENTHANDLE(xxx) (std::bind(&xxx, this, std::placeholders::_1))

HttpProxyConnection::HttpProxyConnection(std::shared_ptr<Acceptor> connfactptr, const IDType id): Connection(connfactptr, id) {
    serverptr_ = std::shared_ptr<Socket>(new Socket);
    udpptr_ = std::shared_ptr<Socket>(new Socket);
}

int HttpProxyConnection::switchBuffers() {
    auto ibufptr = m_client.getInBufferPtr();
    m_server.setOutBufferPtr(ibufptr);
    ibufptr = m_server.getInBufferPtr();
    m_client.setOutBufferPtr(ibufptr);
    return 0;
}


int HttpProxyConnection::closeHandle(Socket& sock) {
    /*
    socl.disableEvent(SOCKET_EVENT_WRITE | SOCKET_EVENT_READ);
    int ret = sock.close();
    if(ret != 0) {
        int event = SOCKET_EVENT_NONE;
        if(sock.SSLWantReadMore()) { event = SOCKET_EVENT_READ; }
        else if(sock.SSLWantWriteMore()) { event = SOCKET_EVENT_WRITE; }

        sock.setEventHandle(BIND_EVENTHANDLE(HttpProxyConnection::closeHandle), event);
        sock.enableEvent(event);
        return EVENT_CALLBACK_CONTINUE;
    }

    //client and server socket close success
    if(!m_client.exist() && !m_server.exist()) {
        reset(); 
        return EVENT_CALLBACK_BREAK;
    }

    //client closed, close server socket
    if(sock == m_client) {
        int event = SOCKET_EVENT_READ | SOCKET_EVENT_WRITE | SOCKET_EVENT_RDHUP;
        m_server.setEventHandle(BIND_EVENTHANDLE(HttpProxyConnection::closeHandle), event);
        m_server.enableEvent(event);
        return EVENT_CALLBACK_BREAK;
    }
    else if(sock == m_server) {
        auto obufptr = m_client.getOutBufferPtr();
        if(obufptr.getReadableBytes() > 0) {
            m_client.enableEvent(SOCKET_EVENT_WRITE);
        }
    }
    */
    return EVENT_CALLBACK_CONTINUE;
}

int HttpProxyConnection::readHandle(Socket& sock) {
    /*
    int ret = sock.recvNIO();
    if(TIGERSO_IO_ERROR == ret) {
        if(sock.isSSL()) {
            int event = SOCKET_EVENT_NONE;
            if(sock.SSLNeedReadMore()) { event = SOCKET_EVENT_READ; }
            else if(sock.SSLNeedWriteMore()) { event = SOCKET_EVENT_WRITE; }
            else { return closeHandle(sock); }
            sock.setEventHandle(BIND_EVENTHANDLE(HttpProxyConnection::readHandle), event);
            sock.disableEvent(SOCKET_EVENT_READ);
            sock.enableEvent(event);
            return EVENT_CALLBACK_CONTINUE;
        }
        return closeHandle(sock);
    }

    if(sock == m_client) {
         

    }
    else if(sock == m_server) {

    }
    */
    
    return EVENT_CALLBACK_CONTINUE;
}

int HttpProxyConnection::writeHandle(Socket& sock) {
    /*
    int ret = sock.sendNIO();
    if(TIGERSO_IO_ERROR == ret) {
        if(sock.isSSL()) {
            int event = SOCKET_EVENT_NONE;
            if(sock.SSLNeedReadMore()) { event = SOCKET_EVENT_READ; }
            else if(sock.SSLNeedWriteMore()) { event = SOCKET_EVENT_WRITE; }
            else { return closeHandle(sock); }
            sock.setEventHandle(BIND_EVENTHANDLE(HttpProxyConnection::writeHandle), event);
            sock.enableEvent(event);
            return EVENT_CALLBACK_CONTINUE;
        }
        return closeHandle(sock);
    }

    auto obufptr = sock.getOutBufferPtr();
    if(obufptr->getReadableBytes() > 0) {
        return EVENT_CALLBACK_CONTINUE;
    }
    sock.disableEvent(SOCKET_EVENT_WRITE);
    sock.setEventHandle(BIND_EVENTHANDLE(HttpProxyConnection::readHandle), SOCKET_EVENT_READ);
    sock.enableEvent(SOCKET_EVENT_READ);
    */
    return EVENT_CALLBACK_CONTINUE;
}

int HttpProxyConnection::errorHandle(Socket& sock) {
    return closeHandle(sock);
}

int HttpProxyConnection::rdhupHandle(Socket& sock) {
    return closeHandle(sock);
}

int HttpProxyConnection::timeoutHandle(Socket& sock) {
    return closeHandle(sock);
}

/*
int HttpProxyConnection::clientRequestHandle(const int recvn) {
    if(state_ != RECV_REQUEST) { return 1; }

    auto ibufptr = m_client.getInBufferPtr();
    int parsedn = parser_.parse(ibufptr->getReadPtr() + ibufptr->getReadableBytes() - recvn, recvn, request_);

    if(parsedn != recvn) {
        closeHandle(m_server);
        HttpHelper::prepare400Response(response_);
        m_client.getOutBufferPtr()->attachHttpMessage(response_);
        m_client.disableEvent(SOCKET_EVENT_READ);
        m_client.enableEvent(SOCKET_EVENT_WRITE);
        return EVENT_CALLBACK_BREAK;
    }
    
    if(parser_.needMoreData()) { return EVENT_CALLBACK_CONTINUE; }
    parser_.reset();


    std::string host = request_.getHost();
    std::string port = request_.getHostPort();
    std::string ipv4;

    //host is IPv4 address
    if(SocketUtil::ValidateAddr(host)) { ipv4 = host; }

    
    
    if(m_server.exist() && )
    
}
*/

void HttpProxyConnection::reset() {
    m_client.reset();
    m_server.reset();
    resetState();
    recycle();
}

void HttpProxyConnection::resetState() {
    state_ = STATE_RESET;
}


}//namespace tigerso
