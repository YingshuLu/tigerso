#include <string>
#include "core/tigerso.h"
#include "core/Logging.h"
#include "net/Socket.h"
#include "net/SocketUtil.h"
#include "net/Channel.h"

namespace tigerso::net {

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
    return sockfd_ > 0;
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

std::shared_ptr<Channel>& Socket::getChannel() { 
    return channelPtr_;
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

    flags = unblock? (flags | O_NONBLOCK): (flags ^ O_NONBLOCK);
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

void Socket::setChannel(std::shared_ptr<Channel>& ptr) {
    channelPtr_ = ptr;
}

ssize_t Socket::recvNIO() {
    setNIO(true);
    auto ptr = bufPtr_.in_.lock();
    if( !ptr || !this->exist() ) {
        return -1;
    }
    return ptr->recvNIO(sockfd_);
}

ssize_t Socket::recvBIO() {
    setNIO(false);
    auto ptr = bufPtr_.in_.lock();
    if( !ptr || this->exist() ) {
        return -1;
    }
    return ptr->recvBIO(sockfd_);
}

ssize_t Socket::sendNIO() {
    setNIO(true);
    auto ptr = bufPtr_.out_.lock();
    if( !ptr || !this->exist() ) {
        return -1;
    }
    return ptr->sendNIO(sockfd_);
}

ssize_t Socket::sendBIO() {
    setNIO(false);
    auto ptr = bufPtr_.out_.lock();
    if( !ptr || !this->exist() ) {
        return -1;
    }
    return ptr->sendBIO(sockfd_);
}

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

int Socket::close() {
    if(!this->exist()) { return 0; }

    std::shared_ptr<Channel> cnptr = this->getChannel();
    if(cnptr != nullptr) {
        cnptr->remove();
    }
    //clear channelptr
    channelPtr_ = nullptr;
    return SocketUtil::Close(*this);
}
 
}
