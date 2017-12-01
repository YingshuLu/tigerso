#ifndef TS_NET_BUFFER_H_
#define TS_NET_BUFFER_H_

#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <iostream>
#include <string>
#include <vector>
#include "core/BaseClass.h"
#include "core/File.h"

namespace tigerso::net {

const int SOCKET_IOSTATE_CLOSED   = -1;
const int SOCKET_IOSTATE_CONTINUE = 0;
const int SOCKET_IOSTATE_ERROR    = -2;

//256KB
#define BUFFER_LEAST_LENGTH (256*1024)
#define BUFFER_GAP_LENGTH (8*1024)

class Buffer: public core::nocopyable {

public:
    typedef int socket_t;
    explicit Buffer(const size_t size = least_len);
    ~Buffer();
    const char* getReadPtr()const;
    ssize_t getReadableBytes()const;
    ssize_t recvBIO(const socket_t);
    ssize_t recvNIO(const socket_t);
    ssize_t sendBIO(const socket_t);
    ssize_t sendNIO(const socket_t );
    std::string toString() const; 
    size_t addData(const char*, size_t);
    size_t addData(const std::string&);
    size_t removeData(std::string&, const size_t);
    size_t clear();
    core::File* getFilePtr() { return &file_; }

private:
    size_t readableBytes() const;
    size_t writeableBytes() const;
    size_t prefreeBytes() const;
    size_t getReadIdx() const;
    size_t getWriteIdx() const;  
    int makeSpace(const size_t len = gain_gap);
    int align();
    void printInfo() const;

private:
    const size_t prefix_;
    size_t readIdx_;
    size_t writeIdx_;
    char* buffer_;
    size_t bufsize_;
    core::File file_;

    static const size_t pregap = 8;
    //initilize 256KB size
    static const size_t least_len = BUFFER_LEAST_LENGTH;
    //gain space with 8KB stepsize
    static const size_t gain_gap = BUFFER_GAP_LENGTH;
};

} //namespace tigerso::net
#endif //TS_NET_BUFFER_H_
