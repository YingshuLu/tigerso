#include "File.h"
#include "RingBuffer.h"

class HttpBodyFile {

public:
    
    int writeIn(const char* buf, size_t len); 
    int send2Socket(int sockfd);
    size_t size() { return _file.getFileSize() + _ringbuf.size(); }

    void endFile() {
        if(_ringbuf.size()) {
        }
    }
public:
    char filename[1024];
    char content_type [1024];
    bool chunked = false; 

private:
    File _file;
    RingBuffer _ringbuf;
};
