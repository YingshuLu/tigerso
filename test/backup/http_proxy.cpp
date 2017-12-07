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

using namespace std;
using namespace tigerso::net;
using namespace tigerso::http;
using namespace tigerso::dns;

#define ERR_HANDLE(x, str) do {   \
    if(!x) {cout << str << endl; abort();} \
}while(0)

#define DBG_LOG(str) do{ cout << str << endl; }while(0)

typedef std::shared_ptr<Socket> SOCKETPTR;

typedef unsigned long IDTYPE;  

class HttpProxyConnection {

typedef std::function<int(HttpProxyConnection&)> LOOP_CALLBACK ;
#define _clientSocket (*csockptr)
#define _serverSocket (*ssockptr)
#define _dnsSocket    (*dsockptr)

#define BIND_EVENTHANDLE(xxx) (std::bind(&xxx, this, std::placeholders::_1))
#define PROXY_LOG(str) do{cout << "P:"<< ID_ <<"|C:" << _clientSocket.getSocket() <<"|S:" << _serverSocket.getSocket() << " [" << str << "]" <<endl; }while(0)
struct HttpTransaction {
    HttpRequest request;
    HttpResponse response;
};

public:
    HttpProxyConnection(): ID_(HttpProxyConnection::uuid()), 
    csockptr(std::make_shared<Socket>()),
    ssockptr(std::make_shared<Socket>()),
    dsockptr(std::make_shared<Socket>()) {

    }

    const IDTYPE& getID() { return ID_; }

    ~HttpProxyConnection() {
        PROXY_LOG(">>>> NOTE, HttpProxyConnection destory");
    }
/*Socket close handle*/
public:
    /*safe close will close socket after all epoll event handled, it is a delay*/
   int clientSafeClose(Socket& client) {
        if(!client.exist()) { return serverSafeClose(_serverSocket); } 
        socketSetEventHandle(client, BIND_EVENTHANDLE(HttpProxyConnection::clientCloseHandle), SOCKET_EVENT_AFTER);
        client2close_ = true;
        return EVENT_CALLBACK_CONTINUE;
   }

   int serverSafeClose(Socket& server) {
        if(!server.exist()) { return EVENT_CALLBACK_BREAK; }
        socketSetEventHandle(server, BIND_EVENTHANDLE(HttpProxyConnection::serverCloseHandle), SOCKET_EVENT_AFTER);
        server2close_ = true;
        return EVENT_CALLBACK_CONTINUE;
   }

  // Only for Before/Error/After Events
  int clientCloseHandle(Socket& client) {
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
  int serverCloseHandle(Socket& server) {
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

    void closeProxyConnection() {
        //First close server
        serverSafeClose(_serverSocket);
        clientSafeClose(_clientSocket);
    }

    // should stop event loop after called this to avoid NULL pointer coredump
    void forceCloseProxyConnection() {
        _clientSocket.close();
        _serverSocket.close();
        finalize();
    }

    int socketNullHandle(Socket& sock) {
        return EVENT_CALLBACK_CONTINUE;
    }

public:
    int clientRDHUPHandle(Socket& client) {
        socketDisableWriteEvent(client);
        clientSafeClose(client);
        return EVENT_CALLBACK_CONTINUE;
    }

    int serverRDHUPHandle(Socket& server) {
        socketDisableWriteEvent(server);
        serverSafeClose(server);
        return EVENT_CALLBACK_CONTINUE;
    }

    int clientErrorHandle(Socket& client) {  return clientRDHUPHandle(client); }
    int serverErrorHandle(Socket& server) {  return serverRDHUPHandle(server); }

    int clientFinalWriteHandle(Socket& client) {
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

public:
    int clientFirstReadHandle(Socket& client) {
        PROXY_LOG("Enter client first read request");
        int recvn = client.recvNIO();
        if(recvn < 0) {
            PROXY_LOG("Client first read request, recv failed!");
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
                PROXY_LOG("Client first read, should not be here");
            }
        }

        PROXY_LOG( "client First Read recv " << recvn << " bytes");
        
        auto ibufptr = client.getInBufferPtr();
        HttpRequest& request = ctransaction_.request;

        const char* readptr = ibufptr->getReadPtr(); 
        const char* parseptr = readptr + ibufptr->getReadableBytes() - recvn;

        int parsedn = cparser_.parse(parseptr, recvn, request);

        //Bad Request
        if(parsedn != recvn) {
            cout << "First Http Parser error: " << cparser_.getStrErr() << endl;
            //Need response "400 bad request" html
            HttpResponse response;
            HttpHelper::prepare400Response(response);
            client.getOutBufferPtr()->addData(response.toString());
            socketSetEventHandle(client, BIND_EVENTHANDLE(HttpProxyConnection::clientWriteHandle), SOCKET_EVENT_WRITE);
            socketDisableReadEvent(client);
            socketEnableWriteEvent(client);
            return EVENT_CALLBACK_BREAK;
        }

        //Http Parser need more data to parse
        if(cparser_.needMoreData()) {
             return EVENT_CALLBACK_CONTINUE;
        }

        //parser complete
        cparser_.reset();
        socketDisableReadEvent(client);
        socketDisableWriteEvent(client);

        PROXY_LOG("client parsed first request");
        /*
         *Upstream proxy
         */
        std::string host = request.getHost();
        std::string port = request.getHostPort();
        PROXY_LOG("---- Fist line: " + request.getMethod() + " " + request.getUrl() + " " + request.getVersion());
        PROXY_LOG(request.getHeader());

        std::string ipv4;
        int dns_ret = resolver_.queryDNSCache(host, ipv4);
        //Hit DNS Cache
        if(dns_ret == DNS_OK) {
            PROXY_LOG("DNS Hit Cache, now connect to server");
            time_t ttl;
            resolver_.getAnswer(ipv4, ttl);
            serverConnectTo(ipv4.c_str(), ttl);
            return EVENT_CALLBACK_CONTINUE;
        }

        //Async DNS query
        PROXY_LOG("Async DNS query");
        resolver_.asyncQueryInit(host.c_str(), *dsockptr);
        resolver_.setCallback(std::bind(&HttpProxyConnection::serverConnectTo, this, std::placeholders::_1, std::placeholders::_2));
        resolver_.asyncQueryStart(*elooptr_, *dsockptr);
        
        return EVENT_CALLBACK_CONTINUE;
    }

    int serverConnectTo(const char* ip, time_t ttl) {
       if(ip == nullptr || strlen(ip) == 0) {
            PROXY_LOG("Failed to query DNS record");
            HttpResponse response;
            HttpHelper::prepareDNSErrorResponse(response);
            _clientSocket.getOutBufferPtr()->addData(response.toString());
            socketSetEventHandle(_clientSocket, BIND_EVENTHANDLE(HttpProxyConnection::clientFinalWriteHandle), SOCKET_EVENT_WRITE);
            socketEnableWriteEvent(_clientSocket);
            return EVENT_CALLBACK_BREAK;
       }

       std::string sport = ctransaction_.request.getHostPort();
       int ret = SocketUtil::Connect(_serverSocket, ip, sport);
       if(ret != 0) {
            PROXY_LOG("Failed connect to server: " + std::string(ip) +":" + sport);
            HttpResponse response;
            HttpHelper::prepare503Response(response);
            _clientSocket.getOutBufferPtr()->addData(response.toString());
            socketSetEventHandle(_clientSocket, BIND_EVENTHANDLE(HttpProxyConnection::clientWriteHandle), SOCKET_EVENT_WRITE);
            socketEnableWriteEvent(_clientSocket);
            _serverSocket.close();
            _clientSocket.reset();
            return EVENT_CALLBACK_BREAK;
       }

       PROXY_LOG("Connect to server: " + std::string(ip) +":" + sport);
       _serverSocket.setNIO(true);
       transferProxyBuffer();

       //Register to event loop
       register_func_(_serverSocket);

       socketSetEventHandle(_serverSocket, BIND_EVENTHANDLE(HttpProxyConnection::serverErrorHandle), SOCKET_EVENT_ERROR | SOCKET_EVENT_RDHUP);
       //HTTPS connection
       const char* smethod = ctransaction_.request.getMethod().c_str();
       if(strcasecmp(smethod, "connect") == 0) {
            PROXY_LOG(ctransaction_.request.getHost() + ", Enter HTTPS proxy, now tunnel it");
            HttpResponse response;
            HttpHelper::prepare200Response(response);
            PROXY_LOG(response.toString());
            _clientSocket.getOutBufferPtr()->addData(response.toString());
            _clientSocket.getInBufferPtr()->clear();

            PROXY_LOG("Prepare 200 Connection OK:\n" << response.getHeader());
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

    int serverFirstWriteHandle(Socket& server) {
        if(!SocketUtil::TestConnect(server)) {
            PROXY_LOG("Test server connect: failed");
            //Perpare connect timeout page for client
            HttpResponse response;
            HttpHelper::prepare503Response(response);
            _clientSocket.getOutBufferPtr()->addData(response.toString());
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

        PROXY_LOG( "server first send " << sendn << "bytes");
        if(server.getOutBufferPtr()->getReadableBytes() > 0) {
            return EVENT_CALLBACK_CONTINUE;
        }
        
        PROXY_LOG("server First send completed");
        //send request completed, now read from server
        
        serverDecideSkipBody();
        socketDisableWriteEvent(server);
        socketSetEventHandle(server, BIND_EVENTHANDLE(HttpProxyConnection::serverReadHandle), SOCKET_EVENT_READ);
        socketEnableReadEvent(server);
        return EVENT_CALLBACK_CONTINUE;
    }

    int serverReadHandle(Socket& server) {
        HttpResponse& response = stransaction_.response;
        
        int recvn = server.recvNIO();
        if(recvn < 0) {
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

        PROXY_LOG( "server recv " << recvn << " bytes");
        size_t lastn = sparser_.getLastParsedSize();
        const char* readptr = server.getInBufferPtr()->getReadPtr(); 
        const char* parseptr = readptr + server.getInBufferPtr()->getReadableBytes() - recvn;

        int parsedn = sparser_.parse(parseptr, recvn, response);
        if(recvn != parsedn) {
            PROXY_LOG("parse server response failed: " + std::string(sparser_.getStrErr()));
            socketDisableReadEvent(server);
            return serverSafeClose(server);
        }

        if(sparser_.headerCompleted()) {
            //PROXY_LOG("server response header parsed completed");
            //Handle big file, tunnel it
            if(sparser_.isBigFile()) {
                socketSetEventHandle(_clientSocket, BIND_EVENTHANDLE(HttpProxyConnection::clientOnlyWriteHandle), SOCKET_EVENT_WRITE);
                socketEnableWriteEvent(_clientSocket);
            }
        }

        if(sparser_.needMoreData()) {
            PROXY_LOG("Server need read more data for http parser");
            return EVENT_CALLBACK_CONTINUE;
        }

        PROXY_LOG("server response parsed completed");
        PROXY_LOG(response.getHeader());
        sparser_.reset();
        ctransaction_.request.clear();

        socketSetEventHandle(_clientSocket, BIND_EVENTHANDLE(HttpProxyConnection::clientWriteHandle), SOCKET_EVENT_WRITE);
        socketEnableWriteEvent(_clientSocket);
        return EVENT_CALLBACK_CONTINUE;
        
    }

    int clientOnlyWriteHandle(Socket& client) {
        PROXY_LOG("client only write response");
        int sendn = client.sendNIO();
        if(sendn < 0) {
            return clientSafeClose(client);
        }

        PROXY_LOG( "client send " << sendn << " bytes");
        /*
        if(client.getOutBufferPtr()->getReadableBytes()) {
            return EVENT_CALLBACK_CONTINUE;
        }
        */
        socketDisableWriteEvent(client);
        return EVENT_CALLBACK_CONTINUE;
    }
    
    int clientWriteHandle(Socket& client) {
        PROXY_LOG("client write response");
        int sendn = client.sendNIO();
        if(sendn < 0) {
            return clientSafeClose(client);
        }

        PROXY_LOG( "client send " << sendn << " bytes");
        if(client.getOutBufferPtr()->getReadableBytes()) {
            return EVENT_CALLBACK_CONTINUE;
        }

        PROXY_LOG("client write response completed");
        socketDisableWriteEvent(client);
        socketSetEventHandle(client, BIND_EVENTHANDLE(HttpProxyConnection::clientReadHandle), SOCKET_EVENT_READ);
        socketEnableReadEvent(client);

        sparser_.reset();
        cparser_.reset();
        stransaction_.response.clear();
        return EVENT_CALLBACK_CONTINUE;
    }

    int clientReadHandle(Socket& client) {
        PROXY_LOG("client read request");
        int recvn = client.recvNIO();
        if(recvn < 0) {
            return clientSafeClose(client);
        }

        PROXY_LOG( "client recv " << recvn << "bytes");
        auto ibufptr = client.getInBufferPtr();
        HttpRequest& request = ctransaction_.request;

        const char* readptr = ibufptr->getReadPtr(); 
        const char* parseptr = readptr + ibufptr->getReadableBytes() - recvn;

        int parsedn = cparser_.parse(parseptr, recvn, request);

        //Bad Request
        if(parsedn != recvn) {
            PROXY_LOG("request parser state: " << cparser_.getParseState());
            PROXY_LOG("Http request Parser error: " + std::string(cparser_.getStrErr()));
            PROXY_LOG("Buffer: " + ibufptr->toString());
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
            //PROXY_LOG("Http request header parsed completed, --- "+ request.getMethod() + "" + request.getUrl());
        }

        //Http Parser need more data to parse
        if(cparser_.needMoreData()) {
             return EVENT_CALLBACK_CONTINUE;
        }

        PROXY_LOG("Http request parsed completed, --- " + request.getUrl());
        //parser complete
        cparser_.reset();
        socketDisableReadEvent(client);

        socketSetEventHandle(_serverSocket, BIND_EVENTHANDLE(HttpProxyConnection::serverWriteHandle), SOCKET_EVENT_WRITE);
        socketEnableWriteEvent(_serverSocket);

        return EVENT_CALLBACK_CONTINUE;
    }

    int serverOnlyWriteHandle(Socket& server) {
        PROXY_LOG("server only write request");
        int sendn = server.sendNIO();
        if(sendn < 0) {
            return serverSafeClose(server);
        }

        PROXY_LOG("server send " << sendn << " bytes");
        /*
        if(server.getOutBufferPtr()->getReadableBytes()) {
            return EVENT_CALLBACK_CONTINUE;
        }
        */
        socketDisableWriteEvent(server);
        return EVENT_CALLBACK_CONTINUE;
    }

    int serverWriteHandle(Socket& server) {
        PROXY_LOG("server write request");
        int sendn = server.sendNIO();
        if(sendn < 0) {
            return serverSafeClose(server);
        }

        PROXY_LOG("server send" << sendn << "bytes");
        if(server.getOutBufferPtr()->getReadableBytes()) {
            return EVENT_CALLBACK_CONTINUE;
        }

        PROXY_LOG("server write request completed");
        socketDisableWriteEvent(server);
        socketSetEventHandle(server, BIND_EVENTHANDLE(HttpProxyConnection::serverReadHandle), SOCKET_EVENT_READ);
        socketEnableReadEvent(server);
        serverDecideSkipBody();
        ctransaction_.request.clear();
        return EVENT_CALLBACK_CONTINUE;
    }

    int clientTunnelWriteHandle(Socket& client) {
        if(!client.getOutBufferPtr()->getReadableBytes()) {
            return EVENT_CALLBACK_CONTINUE;
        }

        PROXY_LOG("Client Tunnel write");
        int sendn = client.sendNIO();
        if(sendn < 0) {
            return clientSafeClose(client);
        }

        PROXY_LOG("Client Tunnel write: " << sendn << " bytes");
        if(client.getOutBufferPtr()->getReadableBytes()) {
            return EVENT_CALLBACK_CONTINUE;
        }
        socketDisableWriteEvent(client);
        socketEnableReadEvent(client);
        return EVENT_CALLBACK_CONTINUE;
    }

    int clientTunnelReadHandle(Socket& client) {
        PROXY_LOG("Client Tunnel read");
        int recvn = client.recvNIO();
        if(recvn < 0) {
            return clientSafeClose(client);
        }
        
        PROXY_LOG("Client Tunnel read: " << recvn << " bytes");
        //socketEnableWriteEvent(client);
        //socketEnableReadEvent(_serverSocket);
        socketEnableWriteEvent(_serverSocket);
        return EVENT_CALLBACK_CONTINUE; 
    }

    int serverTunnelWriteHandle(Socket& server) {
        if(!server.getOutBufferPtr()->getReadableBytes()) {
            return EVENT_CALLBACK_CONTINUE;
        }
        PROXY_LOG("Server Tunnel write");
        int sendn = server.sendNIO();
        if(sendn < 0) {
            return serverSafeClose(server);
        }

        PROXY_LOG("Server Tunnel write: " << sendn << " bytes");
        if(server.getOutBufferPtr()->getReadableBytes()) {
            return EVENT_CALLBACK_CONTINUE;
        }
        socketDisableWriteEvent(server);
        socketEnableReadEvent(server);
        //socketEnableWriteEvent(server);
        return EVENT_CALLBACK_CONTINUE;
    }

    int serverTunnelReadHandle(Socket& server) {
        PROXY_LOG("Server Tunnel read");
        int recvn = server.recvNIO();
        if(recvn < 0) {
            return clientSafeClose(server);
        }
        
        PROXY_LOG("Server Tunnel read: " << recvn << " bytes");
        //socketEnableReadEvent(_clientSocket);
        socketEnableWriteEvent(_clientSocket);
        return EVENT_CALLBACK_CONTINUE; 
    }

    void setSocketRegisterFunc(EventFunc f) {register_func_ = f;}
    void setEraseFunc(LOOP_CALLBACK cb) { erase_func_ = cb; }

private:
    //Exchange In/Out Buffer between client and server
    void transferProxyBuffer() {
        _serverSocket.setOutBufferPtr(_clientSocket.getInBufferPtr());
        _clientSocket.setOutBufferPtr(_serverSocket.getInBufferPtr());
    }

    void serverDecideSkipBody() {
        if(strcasecmp(ctransaction_.request.getMethod().c_str(), "head") == 0) {
            sparser_.setSkipBody(true);
        }
        return;
    }

public:
    void setEventsLoopPtr(EventsLoop* loop) {
        elooptr_ = loop;
    }

private:
    //Reset resources and unregister from proxy loop
    void finalize() {
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

    static IDTYPE uuid() {
        base_++;
        if(base_ == ~((IDTYPE)0)) {
            base_ = 0;
        }
        return base_;
    }

private:
    bool socketEnableReadEvent(Socket& sock) {
        return sock.enableEvent(SOCKET_EVENT_READ);
    }

    bool socketEnableWriteEvent(Socket& sock) {
        return sock.enableEvent(SOCKET_EVENT_WRITE);
    }
    
    bool socketDisableReadEvent(Socket& sock) {
        return sock.disableEvent(SOCKET_EVENT_READ);
    }

    bool socketDisableWriteEvent(Socket& sock) {
        return sock.disableEvent(SOCKET_EVENT_WRITE);
    }

    bool socketSetEventHandle(Socket& sock, EventFunc func, unsigned short flag) {
        auto cnptr = sock.channelptr;

        ERR_HANDLE(cnptr, "channel is nullptr");

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
    
    bool isClientAlive() { return _clientSocket.exist(); }
    bool isServerAlive() { return _serverSocket.exist(); }

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
    const SOCKETPTR csockptr;
    const SOCKETPTR ssockptr;
    const SOCKETPTR dsockptr;
};

IDTYPE HttpProxyConnection::base_ = 0;

typedef std::shared_ptr<HttpProxyConnection> HTTPPROXYCONNECTIONPTR;

/*
 * HttpProxyPool start with events loop, and save HttpProxyConnection with shared pointer
 * the HttpProxyConnection instance can be reused on the loop to avoid frequently memory alloc or free.
 *
 */

class HttpProxyLoop {   

public:
    HttpProxyLoop(const std::string& ipaddr, const std::string& port):
        ipaddr_(ipaddr), port_(port), msockptr_(std::make_shared<Socket>()) {
    }

    //Parent call
    int initListenConnection() {
        return listenHttpClientConnection();
    }

    //Child call
    int startLoop() {
       return eloop_.loop();
    }

public:
    
    /*Get current client connection number*/
    int countHttpConnections() {
        return connections_.size() - discardIDs_.size();
    }

public:
     int masterErrorHandle(Socket& master) {
        
        return EVENT_CALLBACK_BREAK;
     }
     
     int masterAfterHandle(Socket& master) {
        return EVENT_CALLBACK_CONTINUE;       
     }

     int acceptHttpClientConnection(Socket& master) {
        bool accepted = false;

        while(true) {
           HTTPPROXYCONNECTIONPTR hcptr = this->makeHttpProxyConnection();
           SOCKETPTR sockptr = hcptr->csockptr;
           int ret = SocketUtil::Accept(master, *sockptr);
           if(ret != 0) {
                cout<< "Accept errno: " << errno << ", :" << strerror(errno) << endl;
                //if(!accepted) { this->clear(); return EVENT_CALLBACK_DROPWAITED; }
                //master reswap
                if(!accepted) { master.close(); master.reset();  initListenConnection(); return EVENT_CALLBACK_DROPWAITED; }
                //discardThisHttpProxyConnection(*hcptr);
                return EVENT_CALLBACK_CONTINUE;
           }

           cout << "----- Connection socket fd [" << sockptr->getSocket() << "]Accepted from client [" << sockptr->getStrAddr() << ":"<< sockptr->getStrPort() << "]" << endl;
           cout << "----- Current connection number: " << countHttpConnections() << endl;
           accepted = true;
           registerSocket(*sockptr);
           sockptr->setNIO(true);
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
     int discardThisHttpProxyConnection(HttpProxyConnection& hpl) {
         discardIDs_.insert(hpl.getID());
         return 0;
     }
 
    int registerSocket(Socket& sock) {
        return eloop_.registerChannel(sock);
    }
   
private:

    int eraseHttpProxyConnection(HttpProxyConnection& hpl) {
        IDTYPE id = hpl.getID();
        auto it = connections_.find(id);
        if(it != connections_.end()) {
            connections_.erase(it);
        }
        return 0;
    }

   HTTPPROXYCONNECTIONPTR makeHttpProxyConnection() {
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

    void registerFunctionsForHttpProxyConnection(HttpProxyConnection& hpc) {
        hpc.setEraseFunc(std::bind(&HttpProxyLoop::discardThisHttpProxyConnection, this, std::placeholders::_1));
        hpc.setSocketRegisterFunc(std::bind(&HttpProxyLoop::registerSocket, this, std::placeholders::_1));
        hpc.setEventsLoopPtr(&eloop_);
    }

    int insertHttpProxyConnectionPtr(HTTPPROXYCONNECTIONPTR& hcptr) {
        IDTYPE id = hcptr->getID();
        cout << ">> HttpProxyConnection [" << id << "] insert into connections" << endl;
        connections_[id] = hcptr;
        return 0;
    }

    // start here,  listening
    int listenHttpClientConnection() {
       int ret = SocketUtil::CreateListenSocket(ipaddr_.c_str(), port_.c_str(), true, *msockptr_);
       if(ret != 0) { return ret; }

       msockptr_->setNIO(true);
       registerSocket(*msockptr_);
       Channel* cnptr = msockptr_->channelptr;
       
       cnptr->setErrorCallback(std::bind(&HttpProxyLoop::masterErrorHandle, this, std::placeholders::_1));
       cnptr->setReadCallback(std::bind(&HttpProxyLoop::acceptHttpClientConnection, this, std::placeholders::_1));
       cnptr->enableReadEvent();

       DBG_LOG("Master socket listening now");
       return 0;
    }

    void clear() {
        for(auto it = connections_.begin(); it != connections_.end(); it++) {
            it->second->forceCloseProxyConnection();
        }
    }

private:
    //strong link with shared pointer, keep connection alive.
    std::map<IDTYPE, HTTPPROXYCONNECTIONPTR> connections_; 
    //mark as discard, recycle it
    std::set<IDTYPE> discardIDs_;

private:
    const SOCKETPTR msockptr_;
    EventsLoop eloop_;
    
private:
    const std::string ipaddr_;
    std::string port_;
};

HttpProxyLoop* g_httpproxypoolptr = nullptr;
int initHttpProxyPool(const char* ipaddr, const int port);
int destoryHttpProxyPool();

int main() {
    initHttpProxyPool("127.0.0.1", 8080);
    g_httpproxypoolptr->initListenConnection();
    g_httpproxypoolptr->startLoop();

    destoryHttpProxyPool();
    return 0;
}

int initHttpProxyPool(const char* ipaddr, const int port) {
    char sport[12] = {0};
    sprintf(sport, "%d", port);
    g_httpproxypoolptr = new HttpProxyLoop(ipaddr, sport);
    //DNSResolver::setPrimaryAddr("10.64.74.0");
    return 0;
}

int destoryHttpProxyPool() {
    delete g_httpproxypoolptr;
    return 0;
}
