#ifndef TS_NET_SOCKET_H_
#define TS_NET_SOCKET_H_

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string>
#include <memory>
#include <openssl/ssl.h>
#include "core/BaseClass.h"
#include "net/Buffer.h"
#include "ssl/SSLContext.h"

namespace tigerso {
#define SOCKET_EVENT_NONE  0x0000
#define SOCKET_EVENT_READ  0x0001
#define SOCKET_EVENT_WRITE   (SOCKET_EVENT_READ << 1)
#define SOCKET_EVENT_BEFORE  (SOCKET_EVENT_READ << 2)
#define SOCKET_EVENT_AFTER   (SOCKET_EVENT_READ << 3)
#define SOCKET_EVENT_ERROR   (SOCKET_EVENT_READ << 4)
#define SOCKET_EVENT_RDHUP   (SOCKET_EVENT_READ << 5)
#define SOCKET_EVENT_TIMEOUT (SOCKET_EVENT_READ << 6)
#define SOCKET_EVENT_ALL (SOCKET_EVENT_READ | SOCKET_EVENT_WRITE | SOCKET_EVENT_BEFORE |SOCKET_EVENT_AFTER |SOCKET_EVENT_ERROR | SOCKET_EVENT_RDHUP)

typedef int socket_t;
typedef int socket_role_t;
typedef int socket_stage_t;

const socket_role_t SOCKET_ROLE_UINIT  = -1;
const socket_role_t SOCKET_ROLE_CLIENT =  0;
const socket_role_t SOCKET_ROLE_SERVER =  1;

const socket_stage_t SOCKET_STAGE_UINIT   = -1;
const socket_stage_t SOCKET_STAGE_SOCKET  =  0;
const socket_stage_t SOCKET_STAGE_BIND    =  1;
const socket_stage_t SOCKET_STAGE_LISTEN  =  2;
const socket_stage_t SOCKET_STAGE_ACCEPT  =  3;
const socket_stage_t SOCKET_STAGE_CONNECT =  4;
const socket_stage_t SOCKET_STAGE_RECV    =  5;
const socket_stage_t SOCKET_STAGE_SEND    =  6;
const socket_stage_t SOCKET_STAGE_CLOSE   =  7;

struct BufferPtr {
    BufferPtr(std::shared_ptr<Buffer>& buffer)
        :in_(buffer),
        out_(buffer) {
    }

    BufferPtr(std::shared_ptr<Buffer>& bufferIn, std::shared_ptr<Buffer>& bufferOut)
        :in_(bufferIn),
        out_(bufferOut) {
    }

    std::weak_ptr<Buffer> in_;
    std::weak_ptr<Buffer> out_;
};

class SocketUtil;
class Channel;

class Socket: public nocopyable {
friend class SocketUtil;
public:
    Socket(): bufPtr_(inBuffer_, outBuffer_) {
    }
    
    //operator reload
    operator int() const { return sockfd_; }
    bool operator==(const Socket&)const;
    bool operator==(const socket_t&) const;
    bool operator<(const Socket&) const;
    bool operator>(const Socket&) const;

    bool exist() const;
    socket_t getSocket() const;
    std::string getStrAddr() const;
    std::string getStrPort() const;
    bool isNIO() const;
    bool isListening() { if(exist() && listening) {return true;} return false; }
    socket_role_t getRole() const;
    socket_stage_t getStage() const;
    int getSockAddr(sockaddr_in&);
   
    void setNIO(bool);
    void setKeepAlive(bool);
    void setTcpNoDelay(bool);
    void setCloseExec(bool);

    ssize_t recvNIO();
    ssize_t recvBIO();
    ssize_t sendNIO();
    ssize_t sendBIO();
    ssize_t sendNIO(std::string&);
    ssize_t sendFile(const std::string&);
    void setSocket(const socket_t&);
    
    std::shared_ptr<Buffer> getInBufferPtr() const { return bufPtr_.in_.lock(); }
    std::shared_ptr<Buffer> getOutBufferPtr() const { return bufPtr_.out_.lock(); }

    void setInBufferPtr(std::shared_ptr<Buffer> inptr) { bufPtr_.in_ = inptr; }
    void setOutBufferPtr(std::shared_ptr<Buffer> outptr) { bufPtr_.out_ = outptr; }
    int perpareSSLContext();
    //connect
    int close();

    void reset();
    ~Socket() { this->close(); }

public:
    bool enableEvent(unsigned short);
    bool disableEvent(unsigned short);

public:
    Channel* channelptr = nullptr;
    /*SSL support*/
    bool isSSL() { return sctx.active(); }
    bool SSLWantReadMore() { return isSSL() && sctx.serrno == SSL_ERROR_WANT_READ; }
    bool SSLWantWriteMore() { return isSSL() && sctx.serrno == SSL_ERROR_WANT_WRITE; }
    int SSLErrno() { return sctx.serrno; }

private:
    void setStrAddr(const std::string&);
    void setStrPort(const std::string&);
    void setRole(const socket_role_t&);
    void setStage(const socket_stage_t&);

private:
    bool blockIO_ = false;
    bool listening = false;
    socket_role_t role_ = SOCKET_ROLE_UINIT;
    socket_stage_t stage_ = SOCKET_STAGE_UINIT;
    std::string addr_;
    std::string port_;
    socket_t sockfd_ = -1;
    std::shared_ptr<Buffer> inBuffer_ = std::make_shared<Buffer>();
    std::shared_ptr<Buffer> outBuffer_ = std::make_shared<Buffer>();
    BufferPtr bufPtr_;
    /*for SSL socket*/
    SSLContext sctx;
};

typedef std::shared_ptr<Socket> SocketPtr;
} //namespace tigerso::net

#endif //TS_NET_SOCKET_H_
