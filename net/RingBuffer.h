#ifndef TS_NET_RINGBUFFER_H_
#define TS_NET_RINGBUFFER_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include "core/File.h"
#include "core/Logging.h"
#include "net/Socket.h"

namespace tigerso {

/*RingBuffer is the light & quick cache designed for charactor read out & write in
 * Support for multi-pthreads / processes operator without any sync method
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *      readptr         writeptr    
 *         |-----data--| |
 *      |0|1|2|3|4|5|6|7|8|9|
 *       |-----<-----<-----|
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

#define RINGBUFFER_NO_SPACE -3
#define RINGBUFFER_NO_DATA  -2

class RingBuffer {
/* MAX SIZE 64KB
 * MIN SIZE 1KB
 * DEFAULT SIZE 8KB
 */
#define RINGBUFFER_MAX_LENGTH 65536
#define RINGBUFFER_MIN_LENGTH 1024
#define RINGBUFFER_DEFAULT_LENGTH 8192

public:
    RingBuffer(const size_t len = RINGBUFFER_DEFAULT_LENGTH);

    size_t size();
    size_t space();
    bool isFull();
    bool isEmpty();
    int writeIn(const char* buf, size_t length);
    int writeInFromFile(File& file);
    int readOut(char* buf, size_t len);
    //Read buffer out to file or blocking socket
    int readOut(int fd);
    int readOut2File(File& file);
    int send2Socket(int sockfd);
    int send2Socket(Socket&);

    void clear();
    ~RingBuffer();
   
private:
    RingBuffer(const RingBuffer&);
    RingBuffer& operator=(const RingBuffer&);

private:
    char* _buffer;
    char* _readptr;
    char* _writeptr;
    size_t _size;
};

}//namespace tigerso::core
#endif
