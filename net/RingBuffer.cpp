#include "net/RingBuffer.h"
#include "net/SocketUtil.h"
#include "core/tigerso.h"

namespace tigerso {

RingBuffer::RingBuffer(const size_t len) {
    if(len > RINGBUFFER_MAX_LENGTH) { _capacity = RINGBUFFER_MAX_LENGTH; }
    else if( len < RINGBUFFER_MIN_LENGTH) { _capacity = RINGBUFFER_MIN_LENGTH; }
    else { _capacity = len; }

    /*reserve one slot to decide if buffer is empty or full.*/
    _capacity += 1;
    _buffer = (char*) malloc(_capacity);
    clear();
}

size_t RingBuffer::size() { return (_capacity - space() - 1); }
bool RingBuffer::isFull() { return (space() == 0); }
bool RingBuffer::isEmpty() { return  (_readptr == _writeptr); }

int RingBuffer::writeIn(const char* buf, size_t length) {
    if(isFull()) { return 0; }
    if(0 == length || nullptr == buf) { return RINGBUFFER_NO_DATA; }

    size_t len = length;
    if(space() < len) { len = space(); }

    DBG_LOG("before write in: readptr: %d, writeptr: %d", _readptr - _buffer, _writeptr - _buffer);
    if(_writeptr >= _readptr) {
        size_t right = _capacity - (_writeptr - _buffer);
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

    DBG_LOG("after write in: readptr: %d, writeptr: %d", _readptr - _buffer, _writeptr - _buffer);
    DBG_LOG("RingBuffer write in %ld bytes", len);
    return len;
}

int RingBuffer::writeInFromFile(File& file) {
    if(isFull()) { 
        INFO_LOG("ringbuffer is full");
        return 0;
    }

    size_t len = space();
    ssize_t readn = 0;
    DBG_LOG("before write in: readptr: %d, writeptr: %d", _readptr - _buffer, _writeptr - _buffer);
    if(_writeptr >= _readptr) {
        ssize_t left = _readptr - _buffer - 1;
        ssize_t right = ((left >= 0)? (_capacity - (_writeptr - _buffer)): (_capacity - (_writeptr - _buffer) - 1));
        left = ((left >= 0)? left: 0);

        DBG_LOG("right size: %d, left size: %ld", right, left);
        readn = file.continuousReadOut(_writeptr, right);
        if(readn < 0) { return TIGERSO_IO_ERROR; }
        else {
             _writeptr += readn;
            //File done
            if(readn < right) { return readn; }
        }
        DBG_LOG("right write in: readptr: %d, writeptr: %d", _readptr - _buffer, _writeptr - _buffer);

        if(left > 0) {
            readn = file.continuousReadOut(_buffer, left);
            if(readn < 0) { return TIGERSO_IO_ERROR; }
            else {
                _writeptr = _buffer + readn;
                //File done
                if(readn < left) { return right + readn; }
            }
            DBG_LOG("left write in: readptr: %d, writeptr: %d", _readptr - _buffer, _writeptr - _buffer);
        }
    }

        /*
        size_t right = _capacity - (_writeptr - _buffer) - 1;

        //write right
        if(right > 0) {
            readn = file.continuousReadOut(_writeptr, right);

            if(readn < 0) { return TIGERSO_IO_ERROR; }
            else {
                _writeptr += readn;
                //File done or ringbuf is full
                if(readn < right || isFull()) { return readn; }
            }

            DBG_LOG("1.after write in: readptr: %d, writeptr: %d", _readptr - _buffer, _writeptr - _buffer);
        }

        if(_readptr != _buffer) {
            //write the last byte
            readn = file.continuousReadOut(_writeptr, 1);
            if(readn < 0) { return TIGERSO_IO_ERROR; }
            else {
                right += readn;
                //Fine Done
                if(readn == 0)  { return right; }
            }

            //write left
            readn = file.continuousReadOut(_buffer, len-right);
            if(readn < 0) { return TIGERSO_IO_ERROR; }
            else {
                _writeptr = _buffer + readn;
                //File done
                if(readn < len - right) { return right + readn; }
            }
        }
    */
    else {
        readn = file.continuousReadOut(_writeptr, len);
        DBG_LOG("2. after write in: readptr: %d, writeptr: %d", _readptr - _buffer, _writeptr - _buffer);
        if(readn < 0) { return TIGERSO_IO_ERROR; }
        else {
            _writeptr += readn;
            //File done
            if(readn < len) { return readn; }
        }
    }
    return len;
}

int RingBuffer::readOut(char* buf, size_t len) {
    if(isEmpty()) { return 0; }
    if(0 == len || nullptr == buf) { return RINGBUFFER_NO_SPACE; }

    size_t readn = len;
    if(len > size()) { readn = size(); }

    if(_writeptr > _readptr) {
        memcpy(buf, _readptr, readn);
        _readptr += readn;
    }
    else {
        size_t right = _capacity - (_readptr - _buffer);
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

    DBG_LOG("RingBuffer read out %ld bytes\n", readn);
    return readn;
}

//Read buffer out to file or blocking socket
int RingBuffer::readOut(int fd) {
    if(isEmpty()) { return 0; }
    if(fd < 0) { return RINGBUFFER_NO_SPACE; }

    int old_flags = fcntl(fd, F_GETFL);
    if(old_flags < 0) { return RINGBUFFER_NO_SPACE; }

    if(fcntl(fd, F_SETFL, old_flags | O_APPEND | O_WRONLY | O_EXCL) < 0) { return RINGBUFFER_NO_SPACE; }

    size_t datasize = size();
    size_t readn = 0;
    int ret = 0;
    while(size()) {
        readn = (_writeptr > _readptr)? size() : (_capacity - (_readptr - _buffer));
        ret = ::write(fd, _readptr, readn);
        if(ret < 0) { return ret; }
        else if( ret < readn) { ::syncfs(fd); } //Flush to filesystem
        _readptr += ret;
        _readptr = _buffer + ((_readptr - _buffer) % _capacity);
    }

    fcntl(fd, F_SETFL, old_flags);
    DBG_LOG("RingBuffer read out %ld bytes to fd [%d]\n", datasize, fd);
    return datasize;
}

int RingBuffer::readOut2File(File& file) {
    if(isEmpty()) { return TIGERSO_IO_OK; }
    size_t datasize = size();
    size_t readn = 0;
    int ret = 0;
    while(size()) {
        DBG_LOG("1. readptr: %d, writeptr: %d", _readptr - _buffer, _writeptr - _buffer);
        DBG_LOG("1. RingBuffer size: %d", size());
        readn = (_writeptr > _readptr)? size() : (_capacity - (_readptr - _buffer));
        ret = file.appendWriteIn(_readptr, readn);
        if(ret < 0) { return TIGERSO_IO_ERROR; }
        _readptr += ret;
        _readptr = _buffer + ((_readptr - _buffer) % _capacity);
        DBG_LOG("2. RingBuffer size: %d", size());
        DBG_LOG("readptr: %d, writeptr: %d", _readptr - _buffer, _writeptr - _buffer);
    }

    DBG_LOG("RingBuffer read out %ld bytes to file\n", datasize);
    return datasize;   
}

int RingBuffer::send2Socket(int sockfd) {
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
        readn = (_writeptr > _readptr)? size() : (_capacity - (_readptr - _buffer));
        ret = ::send(sockfd, _readptr, readn, MSG_DONTWAIT|MSG_NOSIGNAL);
        if(ret < 0) { 
            if(errno == EAGAIN || errno == EWOULDBLOCK) { return datasize - size();}
            return ret;
        }
        _readptr += ret;
        _readptr = _buffer + ((_readptr - _buffer) % _capacity);
    }

    DBG_LOG("RingBuffer send %ld bytes to socket [%d]\n", datasize, sockfd);
    clear();
    return datasize;
}

int RingBuffer::send2Socket(Socket& mcsock) {
    if(isEmpty()) { return 0; }

    size_t datasize = size();
    size_t readn = 0;
    size_t rn;
    int ret = 0;
    while(size()) {
        readn = (_writeptr > _readptr)? size() : (_capacity - (_readptr - _buffer));
        ret = SocketUtil::Send(mcsock, _readptr, readn, &rn);
        if(TIGERSO_IO_ERROR == ret) { return ret; }

        DBG_LOG("In loop RingBuffer send %ld bytes", rn);
        if(TIGERSO_IO_RECALL == ret) {
                DBG_LOG("RingBuffer send %ld bytes, need recall to send more", datasize - size());
                return datasize - size();
        }

        _readptr += rn;
        _readptr = _buffer + ((_readptr - _buffer) % _capacity);
    }
    DBG_LOG("RingBuffer send %ld bytes, send completed", datasize);
    clear();
    return datasize;
}

void RingBuffer::clear() {
    _readptr = _buffer;
    _writeptr = _buffer;
}

RingBuffer::~RingBuffer() {
    free(_buffer);
}

size_t RingBuffer::space() {
    if(_writeptr >= _readptr) { return (_capacity - (_writeptr - _readptr) - 1); }
    else { return (_readptr - _writeptr - 1); }
    //Never be here
    return 0;
}

}//namespace tigerso::net
