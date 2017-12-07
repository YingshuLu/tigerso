#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory>
#include <string>
#include <map>
#include <set>
#include "core/SysUtil.h"
#include "core/File.h"
#include "net/Socket.h"
#include "net/SocketUtil.h"
#include "http/HttpParser.h"
#include "http/HttpMessage.h"
#include "util/"

using namespace tigerso::net;
using namespace tigerso::http;
using namespace tigerso::core;


class staticWebCGI {
    
public:
    staticWebCGI(HttpRquest& request, HttpResponse) {
    }





};


typedef unsigned int UUID_T; 
class HttpConnection {

public:
    HttpConnection(): sockptr(std::make_shared<Socket>()) {}
    
    UUID_T getUid() { return _uuid; }
    int readRequestHandle(Socket& sock);
    int writeResponseHandle(Socket& sock);

public:
    const SocketPtr sockptr;

private:
    HttpConnection(const HttpConnection&);
    HttpConnection& operator=(const HttpConnection&);

private:
    static UUID_T _updateUID();

private:
    HttpParser        _parser;
    HttpRquest        _request;
    HttpResponse      _response;

private:
    UUID_T _uuid = 0;
    static UUID_T _baseID;
};

UUID_T HttpConnection::_baseID = 0;
UUID_T HttpConnection::_updateUID() {
    if(_baseID >= ~(0)) { return _baseID = 0; }
    _uuid = ++_baseID;
    return _uuid;
}

typedef std::shared_ptr<HttpConnection> HTTPCONNECTIONPTR;

class HttpServer {

public:

    int acceptConnections(Socket& master) {
       bool accepted = false;

        while(true) {

            SocketPtr sockptr = _newConnection()->sockptr;
            int ret = SocketUtil::Accept(master, *sockptr);
            if(ret != 0) {
                if(errno != EAGAIN && errno != EWOULDBLOCK) {
                    INFO_LOG("Accept errno: %d, %s", errno, strerror(errno));
                    //master reswap
                    if(!accepted) { return masterErrorHandle(master); }
                    else { }
            }
            return EVENT_CALLBACK_CONTINUE;
        }

        DBG_LOG("----- Connection socket fd [%d], Accepted from client %s:%s", sockptr->getSocket(), sockptr->getStrAddr().c_str(), sockptr->getStrPort().c_str());
        DBG_LOG("----- Current connection number: %d", countConnections());
        accepted = true;
        sockptr->setNIO(true);
        registerSocket(*sockptr);
        Channel* cnptr = sockptr->channelptr;
        cnptr->setReadCallback(std::bind(&HttpConnection::readRequestHandle, &(*hcptr), std::placeholders::_1));
        cnptr->setErrorCallback(std::bind(&HttpProxyConnection::clientErrorHandle, &(*hcptr), std::placeholders::_1));
        cnptr->setRdhupCallback(std::bind(&HttpProxyConnection::clientRDHUPHandle, &(*hcptr), std::placeholders::_1));
        cnptr->enableReadEvent();
    }

    //should not be here
    return EVENT_CALLBACK_CONTINUE;

    }



private:

    int _createListenMaster(const char* ipaddr, int port) {
        char sport[16] = {0};
        sprintf("%d", port);
        int ret = SocketUtil::CreateListenSocket(ipaddr, sport, _masterSock);
         if(ret != 0) {
            INFO_LOG("Create listen socket failed, errno: %d, %s", errno, strerror(errno));     
            return ret; 
        }

        _masterSock.setNIO(true);
        

    }

    HTTPCONNECTIONPTR _newConnection() {
        HTTPCONNECTIONPTR&& hcptr = std::make_shared<HttpConnection>();
        UUID_T uid = hcptr->getUid();
        _connections[uid] = hcptr;
        return hcptr;
    }

    int deleteConnection(HTTPCONNECTIONPTR& hcptr) {
        auto iter = _connections.find(hcptr->getUid());
        if(iter != _connections.end()) {
            _connections.erase(iter);
        }
        return 0;
    }

    int _countConnections() { return _connections.size(); }

    int registerSocket(Socket& socket) {
        _eloop.registerChannel(socket);
        return 0;
    }


private:
    const std::string _host;
    
private:
    EventsLoop _eloop;
    SocketPtr _masterSock;
    std::map<UUID_T, HTTPCONNECTIONPTR> _connections;
};



