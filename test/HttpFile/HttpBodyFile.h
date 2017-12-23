#include <string>
#include <iomanip>
#include "File.h"
#include "RingBuffer.h"

class HttpBodyFile {
typedef  enum _chunk_send_state{
        _chunkuinit,
        _chunksizeon,
        _chunksizedone,
        _chunkedataon,
        _chunkedatadone,
        _chunkeofon,
        _chunkeofdone,
        _chunkfiledone,
        _chunkNullon,
        _chunkNulldone
    } ChunkState;

public:
    HttpBodyFile(const char* filename):_ringbuf(4096), _file(filename) {}

    void setFile(const char* filename) {
        _file.setFilename(filename);
    }
    
    int writeIn(const char* buf, size_t len); 
    int sendContent2Socket(int sockfd) {
        int ret = 0;
        size_t sendn = 0;
        if(!_senddone) {
            ret = _file.send2Socket(sockfd, sendn);
            if(ret < 0) { return FILE_SENDFILE_ERROR; }
            else if(FILE_SENDFILE_DONE == ret) { 
                _senddone = true;
                _ringbuf.clear();
                _ringbuf.writeIn("\r\n", 2);
            }
            else { return FILE_SENDFILE_RECALL; }
         }

         ret = _ringbuf.send2Socket(sockfd);
         if(ret < 0) { return  FILE_SENDFILE_ERROR; }
         if(_ringbuf.size()) {
            return FILE_SENDFILE_RECALL; 
         }
         _senddone = false;
        return FILE_SENDFILE_DONE;
    }

    int sendChunk2Socket(int sockfd) {
        if(_file.getFileSize() == 0) { _chunkstate = _chunkfiledone; }
        size_t leftdata = 0;

        int ret = 0;
        size_t sendn = 0;
        size_t count = 0;
        char size_str[64] = {0};
        while (_chunkstate != _chunkNulldone) {
            leftdata = _file.getFileSize() - _readoffset;
            switch(_chunkstate) {
        
                case _chunkuinit:
                    if(leftdata == 0) {_chunkstate = _chunkfiledone; break;}
                    if(leftdata < _chunksize) { _chunksize = leftdata; }
                    _ringbuf.clear();
                    bzero(size_str, strlen(size_str));
                    sprintf(size_str, "%s\r\n", inter2HexString(_chunksize).c_str());
                    _ringbuf.writeIn(size_str, strlen(size_str));
                    _chunkstate = _chunksizeon;

                case _chunksizeon:
                    ret = _ringbuf.send2Socket(sockfd);
                    if(ret < 0) {  reset(); return FILE_SENDFILE_ERROR; }
                    if(_ringbuf.size()) {
                        _chunkstate = _chunksizeon;
                        return FILE_SENDFILE_RECALL;
                    }
                    _chunkstate = _chunksizedone;

                case _chunksizedone:
                
                case _chunkedataon:
                    sendn = 0;
                    count = _chunksize;
                    ret = _file.send2Socket(sockfd, sendn, _readoffset, count);
                    if(ret < 0) { reset(); return FILE_SENDFILE_ERROR; }
                    else if( ret == FILE_SENDFILE_RECALL) { 
                        _chunkstate = _chunkedataon;
                        _chunksize -= sendn;
                        return ret;
                    }
                    _chunkstate = _chunkedatadone;
                
                case _chunkedatadone:
                    _ringbuf.clear();
                    _ringbuf.writeIn("\r\n", 2);
                    _chunkstate = _chunkeofon;
                    
                case _chunkeofon:
                    ret = _ringbuf.send2Socket(sockfd);
                    if(ret < 0) { reset(); return FILE_SENDFILE_ERROR; }
                    if(FILE_SENDFILE_RECALL == ret) {
                        _chunkstate = _chunkeofon;
                        return FILE_SENDFILE_RECALL;
                    }
                    _chunksize = 4096;
                    if(_file.getFileSize() - _readoffset > 0) {
                        _chunkstate = _chunkuinit;
                        break;
                    }
                    else {
                        _chunkstate = _chunkfiledone;
                    }

                case _chunkfiledone:
                    _ringbuf.clear();
                    _ringbuf.writeIn("0\r\n\r\n", 5);
                    _chunkstate = _chunkNullon;

                case _chunkNullon:
                    ret = _ringbuf.send2Socket(sockfd);
                    if(ret < 0) {  reset(); return FILE_SENDFILE_ERROR; }
                    if(_ringbuf.size()) { _chunkstate = _chunkeofon; return FILE_SENDFILE_RECALL; }
                    _chunkstate = _chunkNulldone;

                case _chunkNulldone:

                default:
                    break;
            }
        }
        reset();
        return FILE_SENDFILE_DONE;
    }

    size_t size() { return _file.getFileSize() + _ringbuf.size(); }

    void endFile() {
        if(_ringbuf.size()) {
            _ringbuf.readOut2File(_file);
        }
        return;
    }

    void reset() {
        _ringbuf.clear();
        _file.reset();
        _readoffset = 0;
        _chunksize = 4096;
        _senddone = false;
        _chunkstate = _chunkuinit;
    }

private:
    std::string inter2HexString(int num) {
        std::stringstream sstm;
        sstm << std::hex << num;
        return std::string(sstm.str());
    }

public:
    char filename[1024];
    char content_type [1024];
    char content_encoding[1024];
    char mine_type[1024];
    bool chunked = false; 

private:
    File _file;
    RingBuffer _ringbuf; // Cache for file
    off_t _readoffset = 0;
    int _chunksize = 4096;
    bool _senddone = false;
    ChunkState _chunkstate = _chunkuinit;

};

