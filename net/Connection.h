#ifndef TS_NET_CONNECTION_H_
#define TS_NET_CONNECTION_H_

#include <string.h>
#include <stdlib.h>
#include <memory>
#include "core/tigerso.h"
#include "core/BaseClass.h"
#include "net/Socket.h"

namespace tigerso {

typedef unsigned int ConnectionType;
typedef unsigned int IDType;

const ConnectionType HTTP_CONNECTION = 0;
const ConnectionType HTTP_PROXY_CONNECTION = 1;
const ConnectionType TCP_UNKNOWN = 2;

class ConnectionFactory;
class Acceptor;

class Connection: public nocopyable {
friend class ConnectionFactory;
friend class Acceptor;
public:
    Connection(std::shared_ptr<Acceptor>, const IDType);

    IDType getID();
    std::shared_ptr<Socket>& getSocketPtr();
    int readHandleEnter(Socket& _l);
    int writeHandleEnter(Socket& _l);
    int errorHandleEnter(Socket& _l);
    int rdhupHandleEnter(Socket& _l);
    int timeoutHandleEnter(Socket& _l);

public:
    virtual int readHandle(Socket& _l) = 0;
    virtual int writeHandle(Socket& _l) = 0;
    virtual int errorHandle(Socket& _l) = 0;
    virtual int rdhupHandle(Socket& _l) = 0;
    virtual int timeoutHandle(Socket& _l) = 0;
    virtual ~Connection();

protected:
    ConnectionType getType();
    virtual void recycle();
    int registerChannel(Socket&);
    void linkToAcceptor(std::shared_ptr<Acceptor>);

protected:
    std::shared_ptr<Socket> sockptr_ = nullptr;
    std::shared_ptr<Acceptor> acptptr_ = nullptr;

private:
    const IDType id_;
};

} //namespace tigerso

#endif
