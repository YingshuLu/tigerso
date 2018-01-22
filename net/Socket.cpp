#include <string>
#include "core/tigerso.h"
#include "core/Logging.h"
#include "net/Socket.h"
#include "net/SocketUtil.h"
#include "net/Channel.h"

namespace tigerso {

bool Socket::operator==(const Socket& sock) const {
    return sockfd_ == sock.getSocket();
}

bool Socket::operator==(const socket_t& fd) const {
    return sockfd_ == fd;
}

bool Socket::operator<(const Socket& sock) const {
    return sockfd_ < sock.getSocket();
}

bool Socket::operator>(const Socket& sock) const {
    return sockfd_ > sock.getSocket();
}

bool Socket::exist() const {
    return validFd(sockfd_);
}

socket_t Socket::getSocket() const {
    return sockfd_;
}

std::string Socket::getStrAddr() const {
    return addr_;
}

std::string Socket::getStrPort() const {
    return port_;
}

bool Socket::isNIO() const {
    return !blockIO_;
}

socket_role_t Socket::getRole() const {
    return role_;
}

socket_stage_t Socket::getStage() const {
    return stage_;
}

int Socket::getSockAddr(sockaddr_in& inaddr) {
    if (!SocketUtil::ValidateAddr(addr_) || SocketUtil::ValidatePort(port_)) {
        return -1;
    }

    inaddr.sin_family = AF_INET;
    inaddr.sin_addr.s_addr = inet_addr(addr_.c_str());
    inaddr.sin_port = htons(atoi(port_.c_str()));
    return 0;
}

void Socket::setSocket(const socket_t& sockfd) {
    sockfd_ = sockfd;
}

void Socket::setStrAddr(const std::string& addr) {
    addr_ = addr;
}

void Socket::setStrPort(const std::string& port) {
    port_ = port;
}

void Socket::setRole(const socket_role_t& role) {
    role_ = role;
}

void Socket::setStage(const socket_stage_t& stage) {
    stage_ = stage;
}

void Socket::setNIO(bool unblock) {
    
    int flags = fcntl(sockfd_, F_GETFL, 0);
    if(flags < 0) {
        DBG_LOG("fcntl get flag failed");
        return;
    }

    flags = unblock? (flags | O_NONBLOCK): (flags & (~O_NONBLOCK));
    if( fcntl(sockfd_, F_SETFL, flags) < 0 ) {
        DBG_LOG("fctnl set flag failed");
        return;
    }
    
    blockIO_ = !unblock;
    return;
}

void Socket::setKeepAlive(bool on) {
    SocketUtil::SetKeepAlive(*this, on);
}

void Socket::setTcpNoDelay(bool on) {
    SocketUtil::SetTcpNoDelay(*this, on);
}

void Socket::setCloseExec(bool on) {
   int flags = fcntl(sockfd_, F_GETFL, 0);
    if(flags < 0) {
        DBG_LOG("fcntl get flag failed");
        return;
    }

    flags = on? (flags | FD_CLOEXEC): (flags ^ FD_CLOEXEC);
    if( fcntl(sockfd_, F_SETFL, flags) < 0 ) {
        DBG_LOG("fctnl set flag failed");
        return;
    }
    return;
}

ssize_t Socket::recvNIO() {
    setNIO(true);
    auto ptr = bufPtr_.in_.lock();
    if( !ptr || !this->exist() ) {
        DBG_LOG("socket is closed");
        return -1;
    }
    //return ptr->recvNIO(sockfd_);
    return ptr->recvFromSocket(*this);
}

/*
ssize_t Socket::recvBIO() {
    setNIO(false);
    auto ptr = bufPtr_.in_.lock();
    if( !ptr || this->exist() ) {
        return -1;
    }
    return ptr->recvBIO(sockfd_);
}
*/

ssize_t Socket::sendNIO() {
    setNIO(true);
    auto ptr = bufPtr_.out_.lock();
    if( !ptr || !this->exist() ) {
        DBG_LOG("socket is closed");
        return -1;
    }
    //return ptr->sendNIO(sockfd_);
    return ptr->sendToSocket(*this);
}
/*
ssize_t Socket::sendBIO() {
    setNIO(false);
    auto ptr = bufPtr_.out_.lock();
    if( !ptr || !this->exist() ) {
        return -1;
    }
    return ptr->sendBIO(sockfd_);
}
*/

/*
ssize_t Socket::sendNIO(std::string& data) {
    setNIO(true);
    if (!exist()) {
        DBG_LOG("socket == -1, invalid");
        return -1;
    }
    else if (data.empty()) {
        DBG_LOG("content data is empty");
        return 0;
    }

    int retcode = 0;

    setStage(SOCKET_STAGE_SEND);

    int length = data.length();

    int n = 0;
    int beg = 0;
    while (!data.empty() && (n = ::send(sockfd_, data.c_str(), data.length(), MSG_DONTWAIT|MSG_NOSIGNAL)) > 0) {
        INFO_LOG("Send: send %d/%d bytes to socket [%d]", n, length, sockfd_);
        beg = n;
        if (n == data.length())
        {
            data.clear();
            break;
        }
    }

    bool nosend = (length == data.length());
    
    if(nosend) {
        if (n == 0) {
            INFO_LOG("Send: socket [%d] has been closed by peer normally", sockfd_);
            this->close();
            retcode = -1;
        }
        else if(n < 0) {
        
            if(errno == EAGAIN || errno == EWOULDBLOCK) {

                INFO_LOG("no free cache, wait next loop");
                retcode = 0;
            }
            else {
                DBG_LOG("socket [%d] has enter error, connection refused, error: %s", sockfd_, strerror(errno));
                this->close();
                retcode = -1;
            }
        }
    }
    else {
        if (data.length() == 0) {

            retcode = length;
        }
        else {
            retcode = length - data.length() + 1;
        }
    }
    return retcode; 
}
*/

int Socket::prepareSSLContext() {
    int ret = 0;
    if(SOCKET_ROLE_CLIENT == role_) {
        ret = sctx.init(SCTX_ROLE_SERVER);
    }
    else if(SOCKET_ROLE_SERVER == role_) {
        ret =  sctx.init(SCTX_ROLE_CLIENT);
    }

    if(ret < 0) { return -1; }
    
    return sctx.bindSocket(sockfd_);
}

int Socket::close() {
    if(isSSL()) { 
        if(sctx.close() == SCTX_IO_RECALL) {
            DBG_LOG("CLOSE SSL need recall");
           return TIGERSO_IO_ERROR;
        }
    }
    /*
    Channel* cnptr = this->channelptr;
    if(cnptr != nullptr) {
        cnptr->remove();
    }
    //clear channelptr
    channelptr = nullptr;
    return SocketUtil::Close(*this);
    */
    return tcpClose();
}

int Socket::tcpClose() {
    sctx.destory();
    Channel* cnptr = this->channelptr;
    if(cnptr != nullptr) {
        cnptr->remove();
    }
    //clear channelptr
    channelptr = nullptr;
    return SocketUtil::Close(*this);
}

void Socket::reset() {
    channelptr = nullptr;
    blockIO_ = false;
    listening = false;
    role_ = SOCKET_ROLE_UINIT;
    stage_ = SOCKET_STAGE_UINIT;
    addr_ = "";
    port_ = "";
    sockfd_ = -1;

    inBuffer_->clear();
    outBuffer_->clear();
    bufPtr_.in_ = inBuffer_;
    bufPtr_.out_ = outBuffer_;
}

bool Socket::enableEvent(unsigned short flags) {
    if(this->exist() && channelptr != nullptr) { 
        if(flags & SOCKET_EVENT_READ) {
            channelptr->enableReadEvent();
        }
        
        if(flags & SOCKET_EVENT_WRITE) {
            channelptr->enableWriteEvent();
        }
        return true;
    }
    return false;
}

bool Socket::enableReadEvent() {
    return enableEvent(SOCKET_EVENT_READ);
}

bool Socket::enableWriteEvent() {
    return enableEvent(SOCKET_EVENT_WRITE);
}

bool Socket::disableReadEvent() {
    return disableEvent(SOCKET_EVENT_READ);
}

bool Socket::disableWriteEvent() {
    return disableEvent(SOCKET_EVENT_WRITE);
}

bool Socket::disableEvent(unsigned short flags) {
    if(this->exist() && channelptr != nullptr) { 
        if(flags & SOCKET_EVENT_READ) {
            channelptr->disableReadEvent();
        }
        
        if(flags & SOCKET_EVENT_WRITE) {
            channelptr->disableWriteEvent();
        }
        return true;
    }
    return false;
}

bool Socket::setEventHandle(EventHandle func, unsigned short flag) {
    auto cnptr = this->channelptr;
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

    if (flag & SOCKET_EVENT_TIMEOUT) {
        cnptr->setTimeoutCallback(func);
    }

    return true;
}

}//namespace tigerso
