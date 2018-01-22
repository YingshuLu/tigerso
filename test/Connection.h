#ifndef TS_NET_CONNECTION_H_
#define TS_NET_CONNECTION_H_

#include <string.h>
#include <stdlib.h>
#include <memory>
#include "core/tigerso.h"
#include "core/BaseClass.h"

namespace tigerso {

class Socket;
class ConnectionFactory;

class Connection: public nocopyable {
public:
    Connection(std::shared_ptr<ConnectionFactory>& connfactptr, const unsigned int id);
    unsigned int getID();
    int readHandleEnter(Socket& _l);
    int writeHandleEnter(Socket& _l);
    int errorHandleEnter(Socket& _l);
    int rdhupHandleEnter(Socket& _l);
    int timeoutHandleEnter(Socket& _l);

    virtual int readHandle(Socket& _l) = 0;
    virtual int writeHandle(Socket& _l) = 0;
    virtual int errorHandle(Socket& _l) = 0;
    virtual int rdhupHandle(Socket& _l) = 0;
    virtual int timeoutHandle(Socket& _l) = 0;

    virtual ~Connection();

protected:
    virtual ConnectionType getType() { return TCP_UNKNOW; }

    viod recyle();
    std::shared_ptr<Socket>& getClientSocket();
    virtual ~Connection();

private:
    std::shared_ptr<Socket> clientptr_ = nullptr;
    std::shared_ptr<ConnectionFactory> connfactptr_ = nullptr;
    unsigned int id_;
};

} //namespace tigerso

#endif
