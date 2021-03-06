#ifndef TS_CORE_FILE_H_
#define TS_CORE_FILE_H_

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <string>
#include "core/BaseClass.h"
#include "core/tigerso.h"
/*
#define nonBlocking(fd) fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK)
#define blocking(fd) fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & (~O_NONBLOCK))
#define validFd(fd) (fd > 0)
*/

namespace tigerso {

#define TEST_FILEACCESS(filename, mode)   (access(filename, mode) == 0)

#define FILE_BIG_CONTENT    256*1024
#define FILE_NAME_MAX_LENGTH    1024
#define FILE_ACTION_OK             0
#define FILE_OPEN_ERROR           -1
#define FILE_WRITE_ACCESS_DENY    -2
#define FILE_READ_ACCESS_DENY     -3
#define FILE_FD_INVALID           -4
#define FILE_ARGS_INVALID         -6

#define FILE_SENDFILE_ERROR       -1
#define FILE_SENDFILE_RECALL       0
#define FILE_SENDFILE_DONE         1 

class File {

public:
    explicit File(const std::string& filename);
    explicit File();
   
    inline bool testExist() { return TEST_FILEACCESS(filename_, F_OK); }
    inline bool testWrite() { return TEST_FILEACCESS(filename_, W_OK); }
    inline bool testRead()  { return TEST_FILEACCESS(filename_, R_OK); }

    static bool exist(const std::string&);
    static bool writeable(const std::string&);
    static bool readable(const std::string&);
    static int size(const std::string&);

    ssize_t readOut(char* buf, size_t len, off_t& offset);
    ssize_t continuousReadOut(char* buf, size_t len);
    ssize_t writeIn(const char* buf, size_t len);
    ssize_t appendWriteIn(const char* buf, size_t len);
    int send2Socket(int sockfd, size_t& sendn);
    int send2Socket(int sockfd, size_t& sendn, off_t& offset, size_t count);
    
    const char* getFilename() const { return (strlen(filename_) > 0? filename_ : NULL); }
    void setFilename(const char* filename);
    inline int unlink() { return ::unlink(filename_); }
    ssize_t getFileSize();

    bool isReadDone() { return readdone_; }
    inline void reset() { this->close(); size_ = 0; cur_ = 0; readdone_= false; }
    void clear() { reset(); bzero(filename_, sizeof(filename_)); }
    int close() { int ret = ::close(fd_); fd_ = -1; }
    ~File();

private:
    inline int setBlockingIO(int block) { if(validFd(fd_)) { return block != 1? nonBlocking(fd_): blocking(fd_); } return -1; }

private:
    File(const File&);
    File& operator=(const File&);

private:
    int fd_;
    off_t size_;
    off_t cur_;
    /* mark read file done for continuous read out method */
    bool readdone_ = false;

private:
    char filename_[FILE_NAME_MAX_LENGTH] = {0};

};



}//namespace tigerso::core

#endif
