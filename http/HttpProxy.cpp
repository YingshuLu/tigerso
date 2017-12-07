#include "http/HttpProxy.h"


namespace tigerso::http{

HttpProxyConnection::HttpProxyConnection(): ID_(HttpProxyConnection::uuid()), 
    csockptr(std::make_shared<Socket>()),
    ssockptr(std::make_shared<Socket>()),
    dsockptr(std::make_shared<Socket>()) {
}

const IDTYPE& HttpProxyConnection::getID() { return ID_; }

HttpProxyConnection::~HttpProxyConnection() {}

int HttpProxyConnection::clientSafeClose(Socket& client) {
    if(!client.exist()) { return serverSafeClose(_serverSocket); } 
    socketSetEventHandle(client, BIND_EVENTHANDLE(HttpProxyConnection::clientCloseHandle), SOCKET_EVENT_AFTER);
    client2close_ = true;
    return EVENT_CALLBACK_CONTINUE;
}

int HttpProxyConnection::serverSafeClose(Socket& server) {
    if(!server.exist()) { return EVENT_CALLBACK_BREAK; }
    socketSetEventHandle(server, BIND_EVENTHANDLE(HttpProxyConnection::serverCloseHandle), SOCKET_EVENT_AFTER);
    server2close_ = true;
    return EVENT_CALLBACK_CONTINUE;
}

int HttpProxyConnection::clientCloseHandle(Socket& client) {
    client.close();
    //Server closed, need finalize this proxy connection
    if(!_serverSocket.exist()) { this->finalize(); return EVENT_CALLBACK_BREAK; }
    socketEnableWriteEvent(_serverSocket);
    socketSetEventHandle(_serverSocket, BIND_EVENTHANDLE(HttpProxyConnection::serverOnlyWriteHandle), SOCKET_EVENT_WRITE);
    socketSetEventHandle(_serverSocket, BIND_EVENTHANDLE(HttpProxyConnection::serverCloseHandle), SOCKET_EVENT_AFTER);
    server2close_ = true;
    return EVENT_CALLBACK_BREAK;
}

// Only for Before/Error/After Events
int HttpProxyConnection::serverCloseHandle(Socket& server) {
    server.close();
    //Client closed, need finalize this proxy connection
    if(!_clientSocket.exist()) { this->finalize(); return EVENT_CALLBACK_BREAK; }
    socketEnableWriteEvent(_clientSocket);
    socketSetEventHandle(_clientSocket, BIND_EVENTHANDLE(HttpProxyConnection::clientOnlyWriteHandle), SOCKET_EVENT_WRITE);
    //client <===> proxy keep alive
    socketSetEventHandle(_clientSocket, BIND_EVENTHANDLE(HttpProxyConnection::clientFirstReadHandle), SOCKET_EVENT_READ);
    socketEnableReadEvent(_clientSocket);
    //client alive, deliver server data to client
    return EVENT_CALLBACK_BREAK;
}

void HttpProxyConnection::closeProxyConnection() {
    //First close server
    serverSafeClose(_serverSocket);
    clientSafeClose(_clientSocket);
}

// should stop event loop after called this to avoid NULL pointer coredump
void HttpProxyConnection::forceCloseProxyConnection() {
    _clientSocket.close();
    _serverSocket.close();
    finalize();
}

int HttpProxyConnection::socketNullHandle(Socket& sock) {
    return EVENT_CALLBACK_CONTINUE;
}

int HttpProxyConnection::clientRDHUPHandle(Socket& client) {
    socketDisableReadEvent(client);
    clientSafeClose(client);
    return EVENT_CALLBACK_CONTINUE;
}

int HttpProxyConnection::serverRDHUPHandle(Socket& server) {
    socketDisableReadEvent(server);
    serverSafeClose(server);
    return EVENT_CALLBACK_CONTINUE;
}

int HttpProxyConnection::clientErrorHandle(Socket& client) {  return clientRDHUPHandle(client); }
int HttpProxyConnection::serverErrorHandle(Socket& server) {  return serverRDHUPHandle(server); }

int HttpProxyConnection::clientFinalWriteHandle(Socket& client) {
    int sendn = client.sendNIO();
    if(sendn < 0) {
        clientSafeClose(client);
        return EVENT_CALLBACK_CONTINUE;
    }

    auto obptr = client.getOutBufferPtr();
    //Need send more data
    if(obptr->getReadableBytes() > 0) {
        //Clear before/after handle in case
        socketSetEventHandle(client, BIND_EVENTHANDLE(HttpProxyConnection::socketNullHandle), SOCKET_EVENT_AFTER | SOCKET_EVENT_BEFORE);
        socketEnableWriteEvent(client);
        //skip after handle
        return EVENT_CALLBACK_BREAK;
    }
    return clientSafeClose(client);
}

int HttpProxyConnection::clientFirstReadHandle(Socket& client) {
    DBG_LOG("Start read client first request");
    int recvn = client.recvNIO();
    if(recvn < 0) {
        INFO_LOG("Client read request failed, errno:%d, %s", errno, strerror(errno));
        socketDisableReadEvent(client);
        closeProxyConnection();
        return EVENT_CALLBACK_CONTINUE;
    }
    else if(recvn == 0) {
        if (cparser_.needMoreData()) {
            //need more data
            return EVENT_CALLBACK_CONTINUE;
        }
        else {
            DBG_LOG("Client read request, should not be here");
        }
    }

    DBG_LOG("Client read %d bytes", recvn);

    auto ibufptr = client.getInBufferPtr();
    HttpRequest& request = ctransaction_.request;

    const char* readptr = ibufptr->getReadPtr(); 
    const char* parseptr = readptr + ibufptr->getReadableBytes() - recvn;

    int parsedn = cparser_.parse(parseptr, recvn, request);

    //Bad Request
    if(parsedn != recvn) {
        INFO_LOG("Http Parser client request error: %s",cparser_.getStrErr());
        //Need response "400 bad request" html
        HttpResponse response;
        HttpHelper::prepare400Response(response);
        client.getOutBufferPtr()->addData(response.toString());
        DBG_LOG("prepare 400 response.\n%s", response.getHeader().c_str());
        socketSetEventHandle(client, BIND_EVENTHANDLE(HttpProxyConnection::clientWriteHandle), SOCKET_EVENT_WRITE);
        socketDisableReadEvent(client);
        socketEnableWriteEvent(client);
        return EVENT_CALLBACK_BREAK;
    }

    //Http Parser need more data to parse
    if(cparser_.needMoreData()) {
        return EVENT_CALLBACK_CONTINUE;
    }

    DBG_LOG("Client first request parsed completely");
    //parser complete
    cparser_.reset();
    socketDisableReadEvent(client);
    socketDisableWriteEvent(client);

    /*
     *Upstream proxy
     */
    std::string host = request.getHost();
    std::string port = request.getHostPort();

    std::string ipv4;
    int dns_ret = resolver_.queryDNSCache(host, ipv4);
    //Hit DNS Cache
    if(dns_ret == DNS_OK) {
        //DBG_LOG("Hit DNS cache, ip address: %d, now connect to server",ipv4.c_str());
        time_t ttl;
        resolver_.getAnswer(ipv4, ttl);
        serverConnectTo(ipv4.c_str(), ttl);
        return EVENT_CALLBACK_CONTINUE;
    }

    //Async DNS query
    DBG_LOG("Start async DNS query ...");
    resolver_.asyncQueryInit(host.c_str(), *dsockptr);
    resolver_.setCallback(std::bind(&HttpProxyConnection::serverConnectTo, this, std::placeholders::_1, std::placeholders::_2));
    resolver_.asyncQueryStart(*elooptr_, *dsockptr);

    return EVENT_CALLBACK_CONTINUE;
}

int HttpProxyConnection::serverConnectTo(const char* ip, time_t ttl) {
    if(ip == nullptr || strlen(ip) == 0) {
        INFO_LOG("Failed to query DNS record");
        HttpResponse response;
        HttpHelper::prepareDNSErrorResponse(response);
        _clientSocket.getOutBufferPtr()->addData(response.toString());
        socketSetEventHandle(_clientSocket, BIND_EVENTHANDLE(HttpProxyConnection::clientFinalWriteHandle), SOCKET_EVENT_WRITE);
        socketEnableWriteEvent(_clientSocket);
        return EVENT_CALLBACK_BREAK;
    }

    std::string sport = ctransaction_.request.getHostPort();
    DBG_LOG("Connect to server: %s:%s", ip, sport.c_str());

    int ret = SocketUtil::Connect(_serverSocket, ip, sport);
    if(ret != 0) {
        INFO_LOG("Failed connect to server: %s:%s", ip, sport.c_str());
        HttpResponse response;
        HttpHelper::prepare503Response(response);
        _clientSocket.getOutBufferPtr()->addData(response.toString());
        socketSetEventHandle(_clientSocket, BIND_EVENTHANDLE(HttpProxyConnection::clientWriteHandle), SOCKET_EVENT_WRITE);
        socketEnableWriteEvent(_clientSocket);
        _serverSocket.close();
        _clientSocket.reset();
        return EVENT_CALLBACK_BREAK;
    }

    _serverSocket.setNIO(true);
    transferProxyBuffer();

    //Register to event loop
    register_func_(_serverSocket);

    socketSetEventHandle(_serverSocket, BIND_EVENTHANDLE(HttpProxyConnection::serverErrorHandle), SOCKET_EVENT_ERROR | SOCKET_EVENT_RDHUP);
    //HTTPS connection
    const char* smethod = ctransaction_.request.getMethod().c_str();
    if(strcasecmp(smethod, "connect") == 0) {
        DBG_LOG("Http method: CONNECT, tunnel the traffic now");
        HttpResponse response;
        HttpHelper::prepare200Response(response);
        _clientSocket.getOutBufferPtr()->addData(response.toString());
        _clientSocket.getInBufferPtr()->clear();

        DBG_LOG("Prepare 200 response:\n%s", response.getHeader().c_str());
        //Tunnel SSL Data
        socketSetEventHandle(_clientSocket, BIND_EVENTHANDLE(HttpProxyConnection::clientTunnelWriteHandle), SOCKET_EVENT_WRITE);
        socketSetEventHandle(_clientSocket, BIND_EVENTHANDLE(HttpProxyConnection::clientTunnelReadHandle), SOCKET_EVENT_READ);
        socketSetEventHandle(_serverSocket, BIND_EVENTHANDLE(HttpProxyConnection::serverTunnelWriteHandle), SOCKET_EVENT_WRITE);
        socketSetEventHandle(_serverSocket, BIND_EVENTHANDLE(HttpProxyConnection::serverTunnelReadHandle), SOCKET_EVENT_READ);
        //Decrypt SSL Data

        socketEnableWriteEvent(_clientSocket);
        return EVENT_CALLBACK_CONTINUE;
    }

    socketSetEventHandle(_serverSocket, BIND_EVENTHANDLE(HttpProxyConnection::serverFirstWriteHandle), SOCKET_EVENT_WRITE);
    socketEnableWriteEvent(_serverSocket);
    return EVENT_CALLBACK_CONTINUE;
}

int HttpProxyConnection::serverFirstWriteHandle(Socket& server) {
    DBG_LOG( "Write request to server");
    if(!SocketUtil::TestConnect(server)) {
        //Perpare connect timeout page for client
        HttpResponse response;
        HttpHelper::prepare503Response(response);
        _clientSocket.getOutBufferPtr()->addData(response.toString());
        INFO_LOG("Fail to connect to server, prepare 503 response.\n%s", response.getHeader().c_str());
        socketSetEventHandle(_clientSocket, BIND_EVENTHANDLE(HttpProxyConnection::clientWriteHandle), SOCKET_EVENT_WRITE);
        socketEnableWriteEvent(_clientSocket);
        _serverSocket.close();
        _serverSocket.reset();
        return EVENT_CALLBACK_BREAK;
    }

    int sendn = server.sendNIO();
    if(sendn < 0) {
        return serverSafeClose(server);
    }

    DBG_LOG( "server write %d bytes", sendn);
    if(server.getOutBufferPtr()->getReadableBytes() > 0) {
        return EVENT_CALLBACK_CONTINUE;
    }

    DBG_LOG("server write completed");
    //send request completed, now read from server

    serverDecideSkipBody();
    socketDisableWriteEvent(server);
    socketSetEventHandle(server, BIND_EVENTHANDLE(HttpProxyConnection::serverReadHandle), SOCKET_EVENT_READ);
    socketEnableReadEvent(server);
    return EVENT_CALLBACK_CONTINUE;
}

int HttpProxyConnection::serverReadHandle(Socket& server) {
    DBG_LOG("Read server response");
    HttpResponse& response = stransaction_.response;

    int recvn = server.recvNIO();
    if(recvn < 0) {
        INFO_LOG("Server read response failed, errno: %d, %s", errno, strerror(errno));
        socketDisableReadEvent(server);
        return serverSafeClose(server);
    }
    else if(recvn == 0) {
        if(sparser_.needMoreData()) {
            return EVENT_CALLBACK_CONTINUE;
        }
        else {
            socketDisableReadEvent(server);
            socketSetEventHandle(_clientSocket, BIND_EVENTHANDLE(HttpProxyConnection::clientWriteHandle), SOCKET_EVENT_WRITE);
            socketEnableWriteEvent(_clientSocket);
            return EVENT_CALLBACK_CONTINUE;
        }
    }

    DBG_LOG( "Server read %d bytes", recvn);
    size_t lastn = sparser_.getLastParsedSize();
    const char* readptr = server.getInBufferPtr()->getReadPtr(); 
    const char* parseptr = readptr + server.getInBufferPtr()->getReadableBytes() - recvn;

    int parsedn = sparser_.parse(parseptr, recvn, response);
    if(recvn != parsedn) {
        INFO_LOG("parse server response failed: %s", sparser_.getStrErr());
        socketDisableReadEvent(server);
        return serverSafeClose(server);
    }

    if(sparser_.headerCompleted()) {
        //Handle big file, tunnel it
        if(sparser_.isBigFile()) {
            socketSetEventHandle(_clientSocket, BIND_EVENTHANDLE(HttpProxyConnection::clientOnlyWriteHandle), SOCKET_EVENT_WRITE);
            socketEnableWriteEvent(_clientSocket);
        }
    }

    if(sparser_.needMoreData()) {
        DBG_LOG("server need read more data");
        return EVENT_CALLBACK_CONTINUE;
    }

    DBG_LOG("server response parsed completed");
    sparser_.reset();
    ctransaction_.request.clear();

    socketSetEventHandle(_clientSocket, BIND_EVENTHANDLE(HttpProxyConnection::clientWriteHandle), SOCKET_EVENT_WRITE);
    socketEnableWriteEvent(_clientSocket);
    return EVENT_CALLBACK_CONTINUE;

}

int HttpProxyConnection::clientOnlyWriteHandle(Socket& client) {
    DBG_LOG("Client only write response");
    int sendn = client.sendNIO();
    if(sendn < 0) { 
        INFO_LOG("Client only write failed, errno: %d, %s", errno, strerror(errno));
        return clientSafeClose(client);
    }

    DBG_LOG( "client write %d bytes", sendn);
    socketDisableWriteEvent(client);
    return EVENT_CALLBACK_CONTINUE;
}

int HttpProxyConnection::clientWriteHandle(Socket& client) {
    DBG_LOG("Client write response");
    int sendn = client.sendNIO();
    if(sendn < 0) {
        INFO_LOG("Client write failed, errno: %d, %s", errno, strerror(errno));
        return clientSafeClose(client);
    }

    DBG_LOG( "client write %d bytes", sendn);
    if(client.getOutBufferPtr()->getReadableBytes()) {
        return EVENT_CALLBACK_CONTINUE;
    }

    DBG_LOG("client write response completed");
    socketDisableWriteEvent(client);
    socketSetEventHandle(client, BIND_EVENTHANDLE(HttpProxyConnection::clientReadHandle), SOCKET_EVENT_READ);
    socketEnableReadEvent(client);

    sparser_.reset();
    cparser_.reset();
    stransaction_.response.clear();
    return EVENT_CALLBACK_CONTINUE;
}

int HttpProxyConnection::clientReadHandle(Socket& client) {
    DBG_LOG("client read request");
    int recvn = client.recvNIO();
    if(recvn < 0) {
        INFO_LOG("Client read request failed, errno: %d, %s", errno, strerror(errno));
        return clientSafeClose(client);
    }
    else if( recvn == 0) { 
        if (cparser_.needMoreData()) {  return EVENT_CALLBACK_CONTINUE; }
        DBG_LOG("Http request parsed completed");
        cparser_.reset();
        socketDisableReadEvent(client);
        socketSetEventHandle(_serverSocket, BIND_EVENTHANDLE(HttpProxyConnection::serverWriteHandle), SOCKET_EVENT_WRITE);
        socketEnableWriteEvent(_serverSocket);
        return EVENT_CALLBACK_CONTINUE;
    }

    DBG_LOG( "client read %d bytes", recvn);
    auto ibufptr = client.getInBufferPtr();
    HttpRequest& request = ctransaction_.request;

    const char* readptr = ibufptr->getReadPtr(); 
    const char* parseptr = readptr + ibufptr->getReadableBytes() - recvn;
    int parsedn = cparser_.parse(parseptr, recvn, request);

    //Bad Request
    if(parsedn != recvn) {
        INFO_LOG("request parser state: %d", cparser_.getParseState());
        INFO_LOG("Http request Parser error: %s", cparser_.getStrErr());
        //Need response "400 bad request" html
        HttpResponse response;
        HttpHelper::prepare400Response(response);
        client.getOutBufferPtr()->addData(response.toString());
        socketSetEventHandle(client, BIND_EVENTHANDLE(HttpProxyConnection::clientWriteHandle), SOCKET_EVENT_WRITE);
        socketDisableReadEvent(client);
        socketEnableWriteEvent(client);
        return EVENT_CALLBACK_BREAK;
    }

    //Header completed
    if(cparser_.headerCompleted()) {
    }

    //Http Parser need more data to parse
    if(cparser_.needMoreData()) {
        DBG_LOG("client need read more data");
        return EVENT_CALLBACK_CONTINUE;
    }

    //parser complete
    DBG_LOG("Http request parsed completed");
    cparser_.reset();
    socketDisableReadEvent(client);
    socketSetEventHandle(_serverSocket, BIND_EVENTHANDLE(HttpProxyConnection::serverWriteHandle), SOCKET_EVENT_WRITE);
    socketEnableWriteEvent(_serverSocket);

    return EVENT_CALLBACK_CONTINUE;
}

int HttpProxyConnection::serverOnlyWriteHandle(Socket& server) {
    DBG_LOG("Server only write request");
    int sendn = server.sendNIO();
    if(sendn < 0) {
        INFO_LOG("Server write failed, errno: %d, %s", errno, strerror(errno));
        return serverSafeClose(server);
    }
    DBG_LOG("server send %d bytes", sendn);
    socketDisableWriteEvent(server);
    return EVENT_CALLBACK_CONTINUE;
}

int HttpProxyConnection::serverWriteHandle(Socket& server) {
    DBG_LOG("server write request");
    int sendn = server.sendNIO();
    if(sendn < 0) {
        INFO_LOG("Server write failed, errno: %d, %s", errno, strerror(errno));
        return serverSafeClose(server);
    }

    DBG_LOG("server send %d bytes", sendn);
    if(server.getOutBufferPtr()->getReadableBytes()) {
        return EVENT_CALLBACK_CONTINUE;
    }

    DBG_LOG("server write request completed");
    socketDisableWriteEvent(server);
    socketSetEventHandle(server, BIND_EVENTHANDLE(HttpProxyConnection::serverReadHandle), SOCKET_EVENT_READ);
    socketEnableReadEvent(server);
    serverDecideSkipBody();
    ctransaction_.request.clear();
    return EVENT_CALLBACK_CONTINUE;
}

int HttpProxyConnection::clientTunnelWriteHandle(Socket& client) {
    if(!client.getOutBufferPtr()->getReadableBytes()) {
        return EVENT_CALLBACK_CONTINUE;
    }

    DBG_LOG("Client Tunnel write");
    int sendn = client.sendNIO();
    if(sendn < 0) {
        INFO_LOG("client write failed, errno: %d, %s", errno, strerror(errno));
        return clientSafeClose(client);
    }

    DBG_LOG("client Tunnel write: %d bytes", sendn);
    if(client.getOutBufferPtr()->getReadableBytes()) {
        return EVENT_CALLBACK_CONTINUE;
    }
    socketDisableWriteEvent(client);
    socketEnableReadEvent(client);
    return EVENT_CALLBACK_CONTINUE;
}

int HttpProxyConnection::clientTunnelReadHandle(Socket& client) {
    DBG_LOG("Client Tunnel read");
    int recvn = client.recvNIO();
    if(recvn < 0) {
        INFO_LOG("Client read failed, errno: %d, %s", errno, strerror(errno));
        return clientSafeClose(client);
    }

    DBG_LOG("Client Tunnel read: %d bytes", recvn);
    socketEnableWriteEvent(_serverSocket);
    return EVENT_CALLBACK_CONTINUE; 
}

int HttpProxyConnection::serverTunnelWriteHandle(Socket& server) {
    if(!server.getOutBufferPtr()->getReadableBytes()) {
        return EVENT_CALLBACK_CONTINUE;
    }
    DBG_LOG("Server Tunnel write");
    int sendn = server.sendNIO();
    if(sendn < 0) {
        INFO_LOG("Server write failed, errno: %d, %s", errno, strerror(errno));
        return serverSafeClose(server);
    }

    DBG_LOG("server Tunnel write: %d bytes", sendn);
    if(server.getOutBufferPtr()->getReadableBytes()) {
        return EVENT_CALLBACK_CONTINUE;
    }
    socketDisableWriteEvent(server);
    socketEnableReadEvent(server);
    return EVENT_CALLBACK_CONTINUE;
}

int HttpProxyConnection::serverTunnelReadHandle(Socket& server) {
    DBG_LOG("Server Tunnel read");
    int recvn = server.recvNIO();
    if(recvn < 0) {
        INFO_LOG("Server read failed, errno: %d, %s", errno, strerror(errno));
        return clientSafeClose(server);
    }

    DBG_LOG("server Tunnel read: %d bytes", recvn);
    socketEnableWriteEvent(_clientSocket);
    return EVENT_CALLBACK_CONTINUE; 
}

void HttpProxyConnection::setSocketRegisterFunc(EventFunc f) {register_func_ = f;}
void HttpProxyConnection::setEraseFunc(LOOP_CALLBACK cb) { erase_func_ = cb; }

//Exchange In/Out Buffer between client and server
void HttpProxyConnection::transferProxyBuffer() {
    _serverSocket.setOutBufferPtr(_clientSocket.getInBufferPtr());
    _clientSocket.setOutBufferPtr(_serverSocket.getInBufferPtr());
}

void HttpProxyConnection::serverDecideSkipBody() {
    if(strcasecmp(ctransaction_.request.getMethod().c_str(), "head") == 0) {
        sparser_.setSkipBody(true);
    }
    return;
}

void HttpProxyConnection::setEventsLoopPtr(EventsLoop* loop) { elooptr_ = loop; }

//Reset resources and unregister from proxy loop
void HttpProxyConnection::finalize() {
    ctransaction_.request.clear();
    stransaction_.response.clear();
    _clientSocket.reset();
    _serverSocket.reset();
    cparser_.reset();
    sparser_.reset();
    keepalive_ = true;
    client2close_ = false;
    server2close_ = false;
    //erase this instance from loop
    erase_func_(*this);
}

IDTYPE HttpProxyConnection::uuid() {
    base_++;
    if(base_ == ~((IDTYPE)0)) {
        base_ = 0;
    }
    return base_;
}

bool HttpProxyConnection::socketEnableReadEvent(Socket& sock) {
    return sock.enableEvent(SOCKET_EVENT_READ);
}

bool HttpProxyConnection::socketEnableWriteEvent(Socket& sock) {
    return sock.enableEvent(SOCKET_EVENT_WRITE);
}

bool HttpProxyConnection::socketDisableReadEvent(Socket& sock) {
    return sock.disableEvent(SOCKET_EVENT_READ);
}

bool HttpProxyConnection::socketDisableWriteEvent(Socket& sock) {
    return sock.disableEvent(SOCKET_EVENT_WRITE);
}

bool HttpProxyConnection::socketSetEventHandle(Socket& sock, EventFunc func, unsigned short flag) {
    auto cnptr = sock.channelptr;
    if(cnptr == nullptr) {
        INFO_LOG("channel is null");
        return false;
    }

    if(flag & SOCKET_EVENT_READ) {
        cnptr->setReadCallback(func);
    }

    if(flag & SOCKET_EVENT_WRITE) {
        cnptr->setWriteCallback(func);
    }

    if (flag & SOCKET_EVENT_ERROR) {
        cnptr->setErrorCallback(func);
    }

    if (flag & SOCKET_EVENT_BEFORE) {
        cnptr->setBeforeCallback(func);
    }

    if (flag & SOCKET_EVENT_AFTER) {
        cnptr->setAfterCallback(func);
    }

    if (flag & SOCKET_EVENT_RDHUP) {
        cnptr->setRdhupCallback(func);
    }

    return true;
}

IDTYPE HttpProxyConnection::base_ = 0;

HttpProxyLoop::HttpProxyLoop(const std::string& ipaddr, const std::string& port):
    ipaddr_(ipaddr), port_(port), msockptr_(std::make_shared<Socket>()) {
}

//Parent call
int HttpProxyLoop::initListenConnection() {
    return listenHttpClientConnection();
}

//Child call
int HttpProxyLoop::startLoop() {
    return eloop_.loop();
}

/*Get current client connection number*/
int HttpProxyLoop::countHttpConnections() {
    return connections_.size() - discardIDs_.size();
}

int HttpProxyLoop::masterErrorHandle(Socket& master) {
    Channel* cnptr = msockptr_->channelptr;
    cnptr->setAfterCallback(std::bind(&HttpProxyLoop::masterAfterHandle, this, std::placeholders::_1));
    return EVENT_CALLBACK_CONTINUE;
}

int HttpProxyLoop::masterAfterHandle(Socket& master) {
    master.close(); 
    master.reset();
    initListenConnection(); 
    return EVENT_CALLBACK_CONTINUE;       
}

int HttpProxyLoop::acceptHttpClientConnection(Socket& master) {
    bool accepted = false;

    while(true) {
        HTTPPROXYCONNECTIONPTR hcptr = this->makeHttpProxyConnection();
        SocketPtr sockptr = hcptr->csockptr;
        int ret = SocketUtil::Accept(master, *sockptr);
        if(ret != 0) {
            if(errno != EAGAIN && errno != EWOULDBLOCK) {
                INFO_LOG("Accept errno: %d, %s", errno, strerror(errno));
                //master reswap
                if(!accepted) { return masterErrorHandle(master);}
                else { }
            }
            return EVENT_CALLBACK_CONTINUE;
        }

        DBG_LOG("----- Connection socket fd [%d], Accepted from client %s:%s", sockptr->getSocket(), sockptr->getStrAddr().c_str(), sockptr->getStrPort().c_str());
        DBG_LOG("----- Current connection number: %d", countHttpConnections());
        accepted = true;
        sockptr->setNIO(true);
        registerSocket(*sockptr);
        Channel* cnptr = sockptr->channelptr;
        cnptr->setReadCallback(std::bind(&HttpProxyConnection::clientFirstReadHandle, &(*hcptr), std::placeholders::_1));
        cnptr->setErrorCallback(std::bind(&HttpProxyConnection::clientErrorHandle, &(*hcptr), std::placeholders::_1));
        cnptr->setRdhupCallback(std::bind(&HttpProxyConnection::clientRDHUPHandle, &(*hcptr), std::placeholders::_1));
        cnptr->enableReadEvent();
    }

    //should not be here
    return EVENT_CALLBACK_CONTINUE;
}

// callback for HttpProxyConnection to relaese itself
int HttpProxyLoop::discardThisHttpProxyConnection(HttpProxyConnection& hpl) {
    discardIDs_.insert(hpl.getID());
    return 0;
}

int HttpProxyLoop::registerSocket(Socket& sock) {
    return eloop_.registerChannel(sock);
}

int HttpProxyLoop::eraseHttpProxyConnection(HttpProxyConnection& hpl) {
    IDTYPE id = hpl.getID();
    auto it = connections_.find(id);
    if(it != connections_.end()) {
        connections_.erase(it);
    }
    return 0;
}

HTTPPROXYCONNECTIONPTR HttpProxyLoop::makeHttpProxyConnection() {
    if(discardIDs_.empty()) {
        HTTPPROXYCONNECTIONPTR hcptr = std::make_shared<HttpProxyConnection>();
        //register functions
        registerFunctionsForHttpProxyConnection(*hcptr);
        //save to keep it alive
        insertHttpProxyConnectionPtr(hcptr);
        return hcptr;
    }

    auto iter = discardIDs_.begin();
    IDTYPE id = *iter;
    assert(connections_.find(id) != connections_.end());
    discardIDs_.erase(iter);
    return connections_[id];
}

void HttpProxyLoop::registerFunctionsForHttpProxyConnection(HttpProxyConnection& hpc) {
    hpc.setEraseFunc(std::bind(&HttpProxyLoop::discardThisHttpProxyConnection, this, std::placeholders::_1));
    hpc.setSocketRegisterFunc(std::bind(&HttpProxyLoop::registerSocket, this, std::placeholders::_1));
    hpc.setEventsLoopPtr(&eloop_);
}

int HttpProxyLoop::insertHttpProxyConnectionPtr(HTTPPROXYCONNECTIONPTR& hcptr) {
    IDTYPE id = hcptr->getID();
    connections_[id] = hcptr;
    return 0;
}

// start here,  listening
int HttpProxyLoop::listenHttpClientConnection() {
    int ret = SocketUtil::CreateListenSocket(ipaddr_.c_str(), port_.c_str(), true, *msockptr_);
    if(ret != 0) {
        INFO_LOG("Create listen socket failed, errno: %d, %s", errno, strerror(errno));     
        return ret; 
    }

    msockptr_->setNIO(true);
    registerSocket(*msockptr_);
    Channel* cnptr = msockptr_->channelptr;

    cnptr->setErrorCallback(std::bind(&HttpProxyLoop::masterErrorHandle, this, std::placeholders::_1));
    cnptr->setReadCallback(std::bind(&HttpProxyLoop::acceptHttpClientConnection, this, std::placeholders::_1));
    cnptr->enableReadEvent();
    DBG_LOG("Master socket listening now");
    return 0;
}

void HttpProxyLoop::clear() {
    for(auto it = connections_.begin(); it != connections_.end(); it++) {
        it->second->forceCloseProxyConnection();
    }
}

} //namespace tigerso::http
