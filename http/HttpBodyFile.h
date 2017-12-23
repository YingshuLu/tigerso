#ifndef TS_HTTP_HTTPBODYFILE_H_
#define TS_HTTP_HTTPBODYFILE_H_

#include <string>
#include <iomanip>
#include "core/File.h"
#include "net/RingBuffer.h"

namespace tigerso {

typedef enum _HTTP_BODY_MODE_ {
    HTTP_BODY_NULL,
    HTTP_BODY_RINGBUFFER,
    HTTP_BODY_FILE
} HttpBodyMode;

class HttpBodyFile {

#define HTTP_FILE_CACHE_SIZE 8192
typedef  enum _CHUNK_SEND_STATE{
        _CHUNKUINIT,
        _CHUNKSIZEON,
        _CHUNKSIZEDONE,
        _CHUNKEDATAON,
        _CHUNKEDATADONE,
        _CHUNKEOFON,
        _CHUNKEOFDONE,
        _CHUNKFILEDONE,
        _CHUNKNULLON,
        _CHUNKNULLDONE
} ChunkState;

public:
    HttpBodyFile();

    HttpBodyMode mode();
    void setFile(const char* filename);   
    int writeIn(const char* buf, size_t len);
    int send2Socket(Socket&);

    size_t size();
    void flushFile();
    void reset();

private:
    std::string inter2HexString(int num);
    int sendFileContent(Socket&);
    int sendFileChunk(Socket&);
    int sendContent(Socket&);
    int sendChunk(Socket&);
    int sendFileContent(int sockfd);
    int sendFileChunk(int sockfd);

public:
    char content_type [1024] = {0};
    char content_encoding[1024] = {0};
    char mime_type[1024] = {0};
    bool chunked = false; 
    static bool sendfile;

private:
    File _file;
    RingBuffer _ringbuf; // Cache for file IO
    off_t _readoffset = 0;
    int _chunksize = 4096;
    bool _sendContentDone = false;
    ChunkState _chunkstate = _CHUNKUINIT;

};

bool HttpBodyFile::sendfile = true;

}//namespace tigerso::http

#endif
