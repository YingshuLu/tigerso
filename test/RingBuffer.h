#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include "File.h"

/*RingBuffer is the light & quick cache designed for charactor read out & write in
 * Support for multi-pthreads / processes operator without any sync method
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *      readptr         writeptr    
 *         |-----data--| |
 *      |0|1|2|3|4|5|6|7|8|9|
 *       |-----<-----<-----|
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

#define RINGBUFFER_NO_SPACE -1
#define RINGBUFFER_NO_DATA  -2

void printchar(char* buf, size_t len) {
    size_t i = 0;
    while(i < len) {
        printf("%c", *(buf + i));
        i++;
    }
    printf("\n");
}

#define POINTER() \
    do { \
        printf("Write id: %ld, Read id: %ld\n",_writeptr - _buffer, _readptr - _buffer); \
    } while(0)

class RingBuffer {
/* MAX SIZE 64KB
 * MIN SIZE 1KB
 * DEFAULT SIZE 8KB
 */
#define RINGBUFFER_MAX_LENGTH 65536
#define RINGBUFFER_MIN_LENGTH 32
#define RINGBUFFER_DEFAULT_LENGTH 8192

public:
    RingBuffer(const size_t len = RINGBUFFER_DEFAULT_LENGTH) {
        if(len > RINGBUFFER_MAX_LENGTH) { _size = RINGBUFFER_MAX_LENGTH; }
        else if( len < RINGBUFFER_MAX_LENGTH) { _size = RINGBUFFER_MIN_LENGTH; }
        else { _size = len; }

        /*reserve one slot to decide if buffer is empty or full.*/
        _size += 1;
        _buffer = (char*) malloc(_size);
        reset();
    }

    size_t size() { return (_size - space() - 1); }
    bool isFull() { return (space() == 0); }
    bool isEmpty() { return  (_readptr == _writeptr); }

    int writeIn(const char* buf, size_t length) {
        if(isFull()) { return 0; }
        if(0 == length || nullptr == buf) { return RINGBUFFER_NO_DATA; }

        size_t len = length;
        if(space() < len) { len = space(); }
        
        if(_writeptr > _readptr) {
            size_t right = _size - (_writeptr - _buffer);
            if(len <= right) { 
                memcpy(_writeptr, buf, len);
                _writeptr += len;
            }
            else {
                memcpy(_writeptr, buf, right);
                memcpy(_buffer, buf + right, len - right);
                _writeptr = _buffer + len - right;
            }
        }
        else {
            memcpy(_writeptr, buf, len);
            _writeptr += len;
        }
        printf("Write in %ld bytes\n", len);
        POINTER();
        printchar(_buffer, _size);
        return len;
    }

    int readOut(char* buf, size_t len) {
        if(isEmpty()) { return 0; }
        if(0 == len || nullptr == buf) { return RINGBUFFER_NO_SPACE; }

        size_t readn = len;
        if(len > size()) { readn = size(); }

        if(_writeptr > _readptr) {
            memcpy(buf, _readptr, readn);
            _readptr += readn;
        }
        else {
            size_t right = _size - (_readptr - _buffer);
            if(readn <= right) {
                memcpy(buf, _readptr, readn);
                _readptr += readn;
            }
            else {
                memcpy(buf, _readptr, right);
                memcpy(buf + right, _buffer, readn - right);
                _readptr = _buffer + readn - right;
            }
        }
        
        printf("Read out %ld bytes\n", readn);
        POINTER();
        printchar(_buffer, _size);
        return readn;
    }

    //Read buffer out to file or blocking socket
    int readOut(int fd) {
        if(isEmpty()) { return 0; }
        if(fd < 0) { return RINGBUFFER_NO_SPACE; }

        int old_flags = fcntl(fd, F_GETFL);
        if(old_flags < 0) { return RINGBUFFER_NO_SPACE; }

        if(fcntl(fd, F_SETFL, old_flags | O_APPEND | O_WRONLY | O_EXCL) < 0) { return RINGBUFFER_NO_SPACE; }

        size_t datasize = size();
        size_t readn = 0;
        int ret = 0;
        while(size()) {
            readn = (_writeptr > _readptr)? size() : (_size - (_readptr - _buffer));
            ret = ::write(fd, _readptr, readn);
            if(ret < 0) { return ret; }
            else if( ret < readn) { ::syncfs(fd); } //Flush to filesystem
            _readptr = _buffer + ((_readptr - _buffer) % _size);
        }
        
        fcntl(fd, F_SETFL, old_flags);
        printf("Read out %ld bytes\n", datasize);
        POINTER();
        return datasize;
    }

    int readOut2File(File& file) {
        if(isEmpty()) { return 0; }
        size_t datasize = size();
        size_t readn = 0;
        int ret = 0;
        while(size()) {
            readn = (_writeptr > _readptr)? size() : (_size - (_readptr - _buffer));
            ret = file.appendWriteIn(_readptr, readn);
            if(ret < 0) { return ret; }
            _readptr = _buffer + ((_readptr - _buffer) % _size);
        }
        
        printf("Read out %ld bytes\n", datasize);
        POINTER();
        return datasize;   
    }

    int send2Socket(int sockfd) {
        if(isEmpty()) { return 0; }
        int flag = ::fcntl(sockfd, F_GETFL);
        if(flag < 0) { return RINGBUFFER_NO_SPACE; }
        int nonblock = flag & O_NONBLOCK;

        //blocking
        if(!nonblock) { return readOut(sockfd); }

        size_t datasize = size();
        size_t readn = 0;
        int ret = 0;
        while(size()) {
            readn = (_writeptr > _readptr)? size() : (_size - (_readptr - _buffer));
            ret = ::send(sockfd, _readptr, readn, MSG_DONTWAIT|MSG_NOSIGNAL);
            if(ret < 0) { 
                if(errno == EAGAIN || errno == EWOULDBLOCK) { return datasize - size();}
                return ret;
            }
            _readptr = _buffer + ((_readptr - _buffer) % _size);
        }
        
        printf("send %ld bytes\n", datasize);
        POINTER();
        return datasize;
    }

    void reset() {
        _readptr = _buffer;
        _writeptr = _buffer;
    }

    ~RingBuffer() {
        free(_buffer);
    }
   
private:
    RingBuffer(const RingBuffer&);
    RingBuffer& operator=(const RingBuffer&);

public:
    size_t space() {
        if(_writeptr >= _readptr) { return (_size - (_writeptr - _readptr) - 1); }
        else { return (_readptr - _writeptr - 1); }
        //Never be here
        return 0;
    }

private:
    char* _buffer;
    char* _readptr;
    char* _writeptr;
    size_t _size;

};
