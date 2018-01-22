#include <stdlib.h>
#include <string.h>
#include <string>
#include <set>
#include  
#include "net/Socket.h"
#include "net/SocketUtil.h"
#include "core/SysUtil.h"

namespace tigerso {

class HttpProxyConnection;
class HttpConnection;

typedef enum {
    HTTP_PROXY_CONNECTION,
    HTTP_CONNECTION,
    TCP_CONNECTION
} ConnectionType;

class Connection {
friend TCPServerLoop;

public:
    Connection(ConnectionFactory* cf, const unsigned int id): id_(id), connfactptr_(cf) {}
    unsigned int getID() { return id_; }

    int readHandleEnter(Socket& _l) {
        return readHandle(_l);
    }

    int writeHandleEnter(Socket& _l) {
        return writeHandle(_l);
    }

    int errorHandleEnter(Socket& _l) {
        return errorHandle(_l);
    }

    int rdhupHandleEnter(Socket& _l) {
        return rdhupHandle(_l);
    }

    int timeoutHandleEnter(Socket& _l) {
        return timeoutHandle(_l);
    }


    virtual int readHandle(Socket& _1) {
        return EVENT_CALLBACK_CONTINUE;
    }

    virtual int writeHandle(Socket& _1) {
        return EVENT_CALLBACK_CONTINUE;
    }
    
    ~Connection() { }

protected:
    viod recyle() {
        if(connfactptr_ != nullptr) {
            connfactptr_.gc_.insert(id_);
        }
        return;
    }

private:
    Connection(const Connection&);
    Connection& operator=(const Connection&);

private:
    SocketPtr clientptr_;
    unsigned int id_;
    ConnectionFactory* connfact_ = nullptr;
};

typedef std::shared_ptr<Connection> ConnectionPtr;

class ConnectionFactory {
friend class Connection;
public:

    ConnectionFactory(EventsLoopPtr& eloop, const ConnectionType type): eloop_(eloop), type_(type) {}
    ConnectionPtr createConnection() {
        auto iter = gc_.begin();
        if(iter != gc_.end()) { 
            gc_.erase(iter);
            return connections_[*iter];
        }

        ConnectionPtr connptr;
        switch (type_) {
            HTTP_CONNECTION:
                connptr = std::shared_ptr<HttpProxyConnection> (new HttpProxyConnection(this, allocID()));
                break;

            HTTP_CONNECTION:
                connptr = std::shared_ptr<HttpConnection>(new HttpConnection(this, allocID()));
                break;

            TCP_CONNECTION:
            default:
                connptr = std::shared_ptr<Connection>(new Connection(this, allocID()));
        }

        connections_.[connptr->getID()] = connptr;
        return connptr;
    }
    

    int countAlive() {
        return connections_.size() - gc_.size();
    }

    int registerChannel(Socket& sock) {
        return eloop_->registerChannel(sock);
    }

private:
    std::map<unsigned int, ConnectionPtr> connections_;
    std::set<unsigned int> gc_;
    ConnectionType type_;
    EventsLoopPtr eloop_;

private:
    static unsigned int allocID() {
        if(idbase_ >= ~(0)) { idbase_ = 0; }
        return ++idbase_;
    }
    static unsigned int idbase_;
   
};

unsigned int ConnectionFactory::idbase_ = 0;

typedef std::unique_ptr<ConnectionFactory> ConnectionFactoryPtr;
typedef std::shared_ptr<EventsLoop EventsLoopPtr;

class TCPServer: public DaemonBase {

public:
    expilict TCPServerLoop(const std::string& name = ""):
        acceptMutex_(name){
    }

    //call before fork
    int init(const std::string& ipaddr, const int port) {
        master_ = std::unique_ptr<Socket>(new Socket);   
        char portstr[64] = {0}
        snprintf(portstr, sizeof(portstr), "%d", port);
        return SocketUtil::CreateListenSocket(ipaddr, portstr, true, *master_);
    }

    int start(ConnectionType type) {
       eloop_ = std::shared_ptr<EventsLoop>(new EventsLoop);
       connfact_ = std::shared_ptr<ConnectionFactory>(new ConnectionFactory(eloop_, type));
       
       eloop_->registerChannel(*master_);
       master_->setEventHandle(BIND_EVENTHANDLE(TCPServerLoop::acceptHandle),SOCKET_EVENT_READ);
       master_->setEventHandle(BIND_EVENTHANDLE(TCPServerLoop::errorHandle), SOCKET_EVENT_ERROR);
       master_->enableEvent(SOCKET_EVENT_READ);
       return 0;
    }

    int errorHandle(Socket& _l) {
        INFO_LOG("listen socket [%d] error: %d, %s", *master_, errno, strerror(errno));
        exit(EXIT_FAILURE);
        return 0;
    }

    int acceptHandle(Socket& _l) {
        if(CONNECTION_LIMIT - connfact_->countAlive() < CONNECTION_LIMIT/16) { return EVENT_CALLBACK_CONTINUE; }

        LockGuard(acceptMutex_);
        ConnectionPtr connptr;
        SocketPtr clientptr;
        while(true) {
            connptr = connfact_->createConnection();
            clientptr = connptr->clientptr_;
            if(SocketUtil::Accept(*master_, *clientptr) != 0) {
                connptr->die();
                if(errno != EAGAIN && errno != EWOULDBLOCK) {
                    INFO_LOG("Accept error: %d, %s", errno, strerror(errno));
                    return errorHandle(*master_); 
                }
                return EVENT_CALLBACK_CONTINUE;
            }
            clientptr->setEventHandle(std::bind(&Connection::readHandleEnter, &(*connptr), std::placeholders::_1), SOCKET_EVENT_READ);
            clientptr->setEventHandle(std::bind(&Connection::errorHandleEnter, &(*connptr), std::placeholders::_1), SOCKET_EVENT_ERROR);
            clientptr->setEventHandle(std::bind(&Connection::rdhupHandleEnter, &(*connptr), std::placeholders::_1), SOCKET_EVENT_RDHUP);
            clientptr->setEventHandle(std::bind(&Connection::timeoutHandleEnter, &(*connptr), std::placeholders::_1), SOCKET_EVENT_TIMEOUT);
        }
    }

    ~TCPServerLoop() {}

    static int CONNECTION_LIMIT;

private:
    TCPServerLoop(const TCPServerLoop&);
    TCPServerLoop& operator=(const TCPServerLoop&);

private:
    std::unique_ptr<Socket> master_;
    EventsLoopPtr eloop_;
    ConnectionFactoryPtr connfact_;
    ShmMutex acceptMutex_;

private:
    const std::string ipaddr_;
    const int port_;

};

}// namespace tigerso
