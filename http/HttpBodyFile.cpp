#include "core/Dechex.h"
#include "http/HttpBodyFile.h"
#include "net/Socket.h"
#include "core/ConfigParser.h"

namespace tigerso {

bool HttpBodyFile::sendfile = true;

HttpBodyFile::HttpBodyFile(const unsigned int bufsize):_ringbuf(bufsize){}

HttpBodyMode HttpBodyFile::mode() {
    if(_file.getFileSize()) {
        flushFile();
        return HTTP_BODY_FILE;
    }
    else if(_ringbuf.size()) {
        return HTTP_BODY_RINGBUFFER;
    }
    return HTTP_BODY_NULL;
}

void HttpBodyFile::setFile(const char* filename) {
    reset();
    /*
    std::string HTTP_BODY_TEMP_PATH = ConfigParser::getInstance()->getValueByKey("http", "tmp_dir");
    _file.setFilename((HTTP_BODY_TEMP_PATH + "/" + filename).c_str());
    */
    _file.setFilename(filename);
}

int HttpBodyFile::writeIn(const char* buf, size_t len) {
    if(len < _ringbuf.space()) {
        return _ringbuf.writeIn(buf, len);
    }
    else if(len >= RINGBUFFER_DEFAULT_LENGTH) {
        flushFile();
        return _file.appendWriteIn(buf, len);
    }
    
    size_t left = len;
    const char* bufbeg = buf;
    int writen = 0;
    while (1) {
        DBG_LOG("left: %u", left);
        if((writen = _ringbuf.writeIn(bufbeg, left)) > 0) {
            bufbeg += writen;
            left -= writen;
        }
        if(writen == RINGBUFFER_NO_DATA) { return 0; }
        if(_ringbuf.isFull()) { flushFile(); }
        //Done
        if(0 == left) { break; }
    }
    return len;
}

int HttpBodyFile::closeFile() {
    flushFile();
    return _file.close();
}

int HttpBodyFile::send2Socket(Socket& mcsock) {
    if(!_file.testExist()) {
        return FILE_SENDFILE_DONE;
    }
    if(!mcsock.isSSL() && HttpBodyFile::sendfile) {
        if(chunked) { return sendFileChunk(mcsock); }
        return sendFileContent(mcsock);
    }

    if(chunked) { return sendChunk(mcsock); }
    return sendContent(mcsock);
}


int HttpBodyFile::sendFileContent(Socket& mcsock) {
    return sendFileContent(mcsock.getSocket());
}

int HttpBodyFile::sendFileChunk(Socket& mcsock) {
    return sendFileChunk(mcsock.getSocket());
}

int HttpBodyFile::sendFileContent(int sockfd) {
    int ret = 0;
    size_t sendn = 0;
    if(!_sendContentDone) {
        ret = _file.send2Socket(sockfd, sendn);
        if(ret < 0) { return FILE_SENDFILE_ERROR; }
        else if(FILE_SENDFILE_DONE == ret) { 
            _sendContentDone = true;
            _ringbuf.clear();
            _ringbuf.writeIn("\r\n", 2);
        }
        else { return FILE_SENDFILE_RECALL; }
     }

     //Send \r\n to flag end content
     ret = _ringbuf.send2Socket(sockfd);
     if(ret < 0) { return  FILE_SENDFILE_ERROR; }
     if(_ringbuf.size()) {
        return FILE_SENDFILE_RECALL; 
     }
     reset();
    return FILE_SENDFILE_DONE;
}

int HttpBodyFile::sendFileChunk(int sockfd) {
    if(_file.getFileSize() == 0) { _chunkstate = _CHUNKFILEDONE; }
    size_t leftdata = 0;

    int ret = 0;
    size_t sendn = 0;
    size_t count = 0;
    char size_str[64] = {0};
    while (_chunkstate != _CHUNKNULLDONE) {
        leftdata = _file.getFileSize() - _readoffset;
        switch(_chunkstate) {
    
            case _CHUNKUINIT:
                if(leftdata == 0) {_chunkstate = _CHUNKFILEDONE; break;}
                if(leftdata < _chunksize) { _chunksize = leftdata; }
                _ringbuf.clear();
                bzero(size_str, strlen(size_str));
                sprintf(size_str, "%s\r\n", inter2HexString(_chunksize).c_str());
                _ringbuf.writeIn(size_str, strlen(size_str));
                _chunkstate = _CHUNKSIZEON;

            case _CHUNKSIZEON:
                ret = _ringbuf.send2Socket(sockfd);
                if(ret < 0) {  reset(); return FILE_SENDFILE_ERROR; }
                if(_ringbuf.size()) {
                    _chunkstate = _CHUNKSIZEON;
                    return FILE_SENDFILE_RECALL;
                }
                _chunkstate = _CHUNKSIZEDONE;

            case _CHUNKSIZEDONE:
            
            case _CHUNKEDATAON:
                sendn = 0;
                count = _chunksize;
                ret = _file.send2Socket(sockfd, sendn, _readoffset, count);
                if(ret < 0) { reset(); return FILE_SENDFILE_ERROR; }
                else if( ret == FILE_SENDFILE_RECALL) { 
                    _chunkstate = _CHUNKEDATAON;
                    _chunksize -= sendn;
                    return ret;
                }
                _chunkstate = _CHUNKEDATADONE;
            
            case _CHUNKEDATADONE:
                _ringbuf.clear();
                _ringbuf.writeIn("\r\n", 2);
                _chunkstate = _CHUNKEOFON;
                
            case _CHUNKEOFON:
                ret = _ringbuf.send2Socket(sockfd);
                if(ret < 0) { reset(); return FILE_SENDFILE_ERROR; }
                if(FILE_SENDFILE_RECALL == ret) {
                    _chunkstate = _CHUNKEOFON;
                    return FILE_SENDFILE_RECALL;
                }
                _chunksize = HTTP_FILE_CACHE_SIZE;
                if(_file.getFileSize() - _readoffset > 0) {
                    _chunkstate = _CHUNKUINIT;
                    break;
                }
                else {
                    _chunkstate = _CHUNKFILEDONE;
                }

            case _CHUNKFILEDONE:
                _ringbuf.clear();
                _ringbuf.writeIn("0\r\n\r\n", 5);
                _chunkstate = _CHUNKNULLON;

            case _CHUNKNULLON:
                ret = _ringbuf.send2Socket(sockfd);
                if(ret < 0) {  reset(); return FILE_SENDFILE_ERROR; }
                if(_ringbuf.size()) { _chunkstate = _CHUNKNULLON; return FILE_SENDFILE_RECALL; }
                _chunkstate = _CHUNKNULLDONE;

            case _CHUNKNULLDONE:

            default:
                break;
        }
    }
    reset();
    return FILE_SENDFILE_DONE;
}

int HttpBodyFile::sendContent(Socket& mcsock) {
    int ret = 0;
    size_t sendn = 0;
    while(!_file.isReadDone()) {
        ret  = _ringbuf.writeInFromFile(_file);
        if(ret < 0) { return FILE_SENDFILE_ERROR; }
        ret = _ringbuf.send2Socket(mcsock);
        if(ret < 0) { return FILE_SENDFILE_ERROR; }
        if(_ringbuf.size()) { return FILE_SENDFILE_RECALL; }

        //content are totally sent in this loop, append EOF, it can be sure RingBuffer has space
        if(_file.isReadDone()) {
            _sendContentDone = true;
            _ringbuf.writeIn("\r\n", 2);
        }
    }

    //always has data in RingBuffer
    while(1) {
        ret = _ringbuf.send2Socket(mcsock);
        if(ret < 0) { return  FILE_SENDFILE_ERROR; }
        if(_ringbuf.size()) {
            return FILE_SENDFILE_RECALL; 
        }
        //RingBuffer is "\r\n"
        if(_sendContentDone) {
            break;
        }
        //left content data sent, now append EOF
        else {
            _sendContentDone = true;
            _ringbuf.writeIn("\r\n", 2);
        }
    }
    reset();
    return FILE_SENDFILE_DONE;
}

int HttpBodyFile::sendChunk(Socket& mcsock) {
    if(_file.getFileSize() == 0) { _chunkstate = _CHUNKFILEDONE; }
    size_t leftdata = 0;

    int ret = 0;
    size_t sendn = 0;
    size_t count = 0;
    char size_str[64] = {0};
    while (_chunkstate != _CHUNKNULLDONE) {
        leftdata = _file.getFileSize() - _readoffset;
        switch(_chunkstate) {
    
            case _CHUNKUINIT:
                if(leftdata == 0) { _chunkstate = _CHUNKFILEDONE;  break; }
                if(leftdata < _chunksize) { _chunksize = leftdata; }
                _ringbuf.clear();
                bzero(size_str, strlen(size_str));
                sprintf(size_str, "%s\r\n", inter2HexString(_chunksize).c_str());
                //perpare chunk size
                _ringbuf.writeIn(size_str, strlen(size_str));
                _chunkstate = _CHUNKSIZEON;

            case _CHUNKSIZEON:
                ret = _ringbuf.send2Socket(mcsock);
                if(ret < 0) {  reset(); return FILE_SENDFILE_ERROR; }
                if(_ringbuf.size()) {
                    _chunkstate = _CHUNKSIZEON;
                    return FILE_SENDFILE_RECALL;
                }
                _chunkstate = _CHUNKSIZEDONE;

            case _CHUNKSIZEDONE:
                //perpare chunk data
                ret = _ringbuf.writeInFromFile(_file);
                if(ret < 0) { reset(); return FILE_SENDFILE_ERROR; }
                _readoffset += ret;
                _chunkstate = _CHUNKSIZEDONE;

            case _CHUNKEDATAON:
                ret = _ringbuf.send2Socket(mcsock);
                if(ret < 0) { reset(); return FILE_SENDFILE_ERROR; }
                if(_ringbuf.size()) {
                    _chunkstate = _CHUNKEDATAON;
                    return FILE_SENDFILE_RECALL;
                }
                _chunkstate = _CHUNKEDATADONE;
            
            case _CHUNKEDATADONE:
                //perpare chunk eof data
                ret = _ringbuf.writeIn("\r\n", 2);
                _chunkstate = _CHUNKEOFON; 
                
            case _CHUNKEOFON:
                ret = _ringbuf.send2Socket(mcsock);
                if(ret < 0) { reset(); return FILE_SENDFILE_ERROR; }
                if(_ringbuf.size()) {
                    _chunkstate = _CHUNKEOFON;
                    return FILE_SENDFILE_RECALL;
                }

                _chunksize = HTTP_FILE_CACHE_SIZE;
                //Has data in file, continue;
                if(_file.getFileSize() - _readoffset > 0) {
                    _chunkstate = _CHUNKUINIT;
                    break;
                }
                else {
                    _chunkstate = _CHUNKFILEDONE;
                }

            case _CHUNKFILEDONE:
                //perpare NULL data to end chunk transfer
                _ringbuf.clear();
                _ringbuf.writeIn("0\r\n\r\n", 5);
                _chunkstate = _CHUNKNULLON;

            case _CHUNKNULLON:
                ret = _ringbuf.send2Socket(mcsock);
                if(ret < 0) {  reset(); return FILE_SENDFILE_ERROR; }
                if(_ringbuf.size()) { _chunkstate = _CHUNKNULLON; return FILE_SENDFILE_RECALL; }
                _chunkstate = _CHUNKNULLDONE;

            case _CHUNKNULLDONE:

            default:
                break;
        }
    }
    reset();
    return FILE_SENDFILE_DONE;
}

size_t HttpBodyFile::size() { return _file.getFileSize() + _ringbuf.size(); }

void HttpBodyFile::flushFile() {
    if(_ringbuf.size()) {
        int ret = _ringbuf.readOut2File(_file);
        DBG_LOG("flush file: %d", ret);
    }
    return;
}

void HttpBodyFile::reset() {
    _ringbuf.clear();
    _file.reset();
    _readoffset = 0;
    _chunksize = HTTP_FILE_CACHE_SIZE;
    _sendContentDone = false;
    _chunkstate = _CHUNKUINIT;
    bzero(content_type, sizeof(content_type));
    bzero(content_encoding, sizeof(content_encoding));
    bzero(mime_type, sizeof(mime_type));
}

std::string HttpBodyFile::inter2HexString(int num) {
    return std::string(dec2hex((unsigned int)num));
}

}//namespace tigerso::http

