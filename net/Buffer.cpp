#include <iostream>
#include "net/Buffer.h"

namespace tigerso::net {

Buffer:: Buffer(const size_t size)
    :prefix_(pregap),
    buffer_(nullptr) {

    readIdx_ = writeIdx_ = prefix_;
    bufsize_ = size;
    if(size < prefix_) {
        bufsize_= least_len;
    }

    buffer_ = (char*) malloc(bufsize_);
    if(buffer_ == nullptr) {
        printf("error: malloc failed(%s)", strerror(errno));
        return;
    }
}

Buffer::~Buffer() {
    if(buffer_ != nullptr) {
        free((void*)buffer_);
        buffer_ = nullptr;
    }
}

const char* Buffer::getReadPtr() const {
    return (buffer_ + getReadIdx());
}

ssize_t Buffer::getReadableBytes() const {
    return readableBytes();
}

ssize_t Buffer::recvBIO(const socket_t fd) {
    if (fd < 0) {
        return -1;
    }

    char buf[65536] = {0};
    iovec vec[2];

    size_t len = writeableBytes();
    size_t buf_len = sizeof(buf);

    vec[0].iov_base = buffer_ + getWriteIdx();
    vec[0].iov_len = writeableBytes();

    vec[1].iov_base = buf;
    vec[1].iov_len = sizeof(buf);

    int iovcnt = (len < buf_len? 2 : 1);

    ssize_t n = 0;

    if((n = readv(fd, vec,iovcnt)) ==  0) {
        return SOCKET_IOSTATE_CLOSED;
    }
    else if (n == -1) {
        return SOCKET_IOSTATE_ERROR;
    }
    else {
        if(n < len) {
            writeIdx_ += n;
        }
        else {
            writeIdx_ += len;
            addData(buf, buf_len);
        }
    }
    return n;
}

ssize_t Buffer::recvNIO(const socket_t sockfd) {
     
    if (sockfd < 0) {
        return -1;
    }

    ssize_t len = 0;
    ssize_t n = 0;

    while ((n = recv(sockfd, (void*) (buffer_ + getWriteIdx()), writeableBytes(), MSG_DONTWAIT)) > 0) {
        writeIdx_ += n;
        len += n;
        if (writeableBytes() < pregap) {
            makeSpace();
        }
    }

    std::cout << ">> ---In Buffer:\n " <<toString() << std::endl;

    if(n == 0) {
        return SOCKET_IOSTATE_CLOSED;
    }
    else if (n == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) { 
            return len;
        }
        else {
            return SOCKET_IOSTATE_ERROR;
        }
    }

    return len;
}

ssize_t Buffer::sendBIO(const socket_t fd) {

    if (fd < 0) {
        return -1;
    }
    if (readableBytes() == 0) {
        return 0;
    }

    ssize_t n = send(fd, (void*) (buffer_+ readIdx_), readableBytes(), MSG_DONTWAIT|MSG_NOSIGNAL);
    if(n > 0) {
        readIdx_ += n;     
        if(writeIdx_ == readIdx_ + 1) {
            align();
        }
        return n;
    }

    return SOCKET_IOSTATE_ERROR;
}

ssize_t Buffer::sendNIO(const socket_t sockfd) {

    if (sockfd < 0) {
        return -1;
    }
    std::cout << ">> --- Out Buffer: " <<toString() << std::endl;
    ssize_t n = 0;
    ssize_t len = 0;
    while(readableBytes() >0 && (n = send(sockfd, (void*) (buffer_  + readIdx_), readableBytes(), MSG_DONTWAIT|MSG_NOSIGNAL)) > 0) {
        len += n;
        readIdx_ += n;     
        if(writeIdx_ == readIdx_ + gain_gap) {
            align();
        }
    }
    if(n == -1) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            return len;
        }
        else {
            return SOCKET_IOSTATE_ERROR;
        }
    }
    return len;
}

std::string Buffer::toString() const {
    size_t start_ptr = readIdx_;
    buffer_[writeIdx_] = '\0';
    return std::string(buffer_ + readIdx_);
}

size_t Buffer::addData(const char* buf, size_t len) {
    if (len <= 0) {
        return 0;
    }
    makeSpace(len);
    memcpy(buffer_ + writeIdx_, buf, len);
    writeIdx_ += len;
    return len;
}

size_t Buffer::addData(const std::string& data) {
    size_t len = data.size();
    if (len <= 0) {
        return 0;
    }
    makeSpace(len);
    memcpy(buffer_ + writeIdx_, data.c_str(), len);
    writeIdx_ += len;
    return len;
}

size_t Buffer::removeData(std::string& data, const size_t len) {

    size_t readableBytes_len = readableBytes();
    size_t remove_len = len;
    if (len > readableBytes_len) {
        remove_len = readableBytes_len;
    }
    size_t start = readIdx_;
    readIdx_ += remove_len;

    char tmp = buffer_[readIdx_];
    buffer_[readIdx_] = '\0';
    data += std::string(buffer_+start);
    buffer_[readIdx_] = tmp;
    printInfo();
    return remove_len;
}

size_t Buffer::clear() {
    readIdx_ = writeIdx_ = prefix_;
    return bufsize_;
}

size_t Buffer::readableBytes() const {
    size_t readableBytes_len = writeIdx_ - readIdx_;
    return readableBytes_len;
}

size_t Buffer::writeableBytes() const {
    size_t len = bufsize_;
    size_t writeableBytes_len = len - writeIdx_;
    return writeableBytes_len;
}   

size_t Buffer::prefreeBytes() const {
    return (readIdx_ - prefix_);
}

size_t Buffer::getReadIdx() const {
    return readIdx_;
}

size_t Buffer::getWriteIdx() const {
    return writeIdx_;
}

int Buffer::makeSpace(const size_t len) {
    align();
    if (writeableBytes() > len) {
        return 0;
    }

    size_t need = len + bufsize_;
    size_t extend = gain_gap;
    while((extend + bufsize_) <= need ) {
        extend = extend << 1;
        std::cout<< "extend : " << extend << std::endl;
    }

    char* new_ptr = (char*) realloc(buffer_, extend + bufsize_);
    if (new_ptr == NULL) {
        return -1;
    }
    buffer_ = new_ptr;
    bufsize_ += extend;
    return 0;
}

int Buffer::align() {
    if (prefreeBytes() < gain_gap || writeableBytes() > readableBytes()) {
        return 0;
    }

    size_t readableBytes_len = readableBytes();
    if (memcpy(buffer_+ prefix_, buffer_+readIdx_, readableBytes_len) != buffer_+prefix_) {
        return -1;
    }

    readIdx_ = prefix_;
    writeIdx_ = readIdx_ + readableBytes_len;
    return 0;
}

void Buffer::printInfo() const {
//#ifdef PRINT_BUFFER_INFO_
    using namespace std;
    cout << "-----------------Buffer Info----------------------"<<endl;
    cout<< "prefix__: " << prefix_<<"\tprefree: "<< prefreeBytes()<<endl;
    cout<< "readIdx_: " << readIdx_<<"\twriteIdx_: " << writeIdx_ << "\tbufsize_: "<< bufsize_<<endl;
    cout<< "readableBytes: " << readableBytes()<<"\twriteableBytes_: " << writeableBytes() <<endl;
    cout<< "buffer: " << toString()<< endl;
    cout << "-----------------Buffer End----------------------"<<endl;
//#endif 
}

} //namespace tigerso::net
