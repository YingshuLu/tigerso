#include <stdio.h>
#include <string.h>
#include <stdlib.h>


/*RingBuffer is the light & quick cache designed for charactor read out & write in
 * Support for multi-pthreads / processes operator without any sync method
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *      readptr       writeptr    
 *         |             |
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
/*MAX SIZE 64KB, MIN SIZE 1KB*/
#define RINGBUFFER_MAX_LENGTH 128
#define RINGBUFFER_MIN_LENGTH 64

public:
    RingBuffer(const size_t& len) {
        if(len > RINGBUFFER_MAX_LENGTH) { _size = RINGBUFFER_MAX_LENGTH; }
        else if( len < RINGBUFFER_MAX_LENGTH) { _size = RINGBUFFER_MIN_LENGTH; }
        else { _size = len; }

        /*reserve one byte to decide if buffer is empty or full.*/
        _size += 1;
        _buffer = (char*) malloc(_size);
        reset();
    }

    size_t size() { return (_size - space() -1); }
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

private:
    size_t space() {
        if(_writeptr >= _readptr) { return (_size - (_writeptr - _readptr) - 1); }
        else { return (_readptr - _writeptr - 1); }
        //Never be here
        return 0;
    }

private:
    char* _buffer;
    size_t _size;
    char* _readptr;
    char* _writeptr;

};
