#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "core/File.h"
#include "core/Logging.h"

namespace tigerso {

File::File() {}

File::File(const char* filename) {
    this->reset();
    size_t len = strlen(filename) < FILE_NAME_MAX_LENGTH? strlen(filename): FILE_NAME_MAX_LENGTH;
    memcpy(filename_, filename, len);
}

ssize_t File::readOut(char* buf, size_t len, off_t& offset) {
    if(NULL == buf || len <= 0) { 
        return FILE_ARGS_INVALID;
    }

    if(!testExist()|| !testRead()) { return FILE_READ_ACCESS_DENY; }

    int rfd = open(filename_, O_RDONLY);
    if(!validFd(rfd)) { return FILE_FD_INVALID; }
    lseek(rfd, offset, SEEK_SET);
    ssize_t readn = ::read(rfd, buf, len);
    offset = lseek(rfd, 0, SEEK_CUR);
    ::close(rfd);
    return readn;
}

ssize_t File::continuousReadOut(char* buf, size_t len) {
    if(NULL == buf || len <= 0) { 
        return FILE_ARGS_INVALID;
    }

    if(!testExist()|| !testRead()) { return FILE_READ_ACCESS_DENY; }

    if(!validFd(fd_)) {
        readdone_ = false;
        fd_ = ::open(filename_, O_RDONLY);
    }
    
    ssize_t readn = ::read(fd_, buf, len);
    if(readn >=0 && readn < len) {
        readdone_ = true;
        reset();
    }
    if(-1 == readn) {
        INFO_LOG("continuousReadOut error: %d, %s", errno, strerror(errno));
    }
    return readn;
}

ssize_t File::writeIn(const char* buf, size_t len) {
    if(NULL == buf || len <= 0) { 
        return FILE_ARGS_INVALID;
    }

    int wfd = ::open(filename_, O_WRONLY|O_CREAT);
    if(!validFd(wfd)) { return FILE_FD_INVALID; }
    //override write
    if(lseek(wfd, 0, SEEK_SET) == -1) { return FILE_FD_INVALID; }
    ssize_t writen = ::write(wfd, buf, len);
    if(writen < len && writen != -1) { ::fsync(wfd); }

    if(-1 == writen) {
        INFO_LOG("continuousReadOut error: %d, %s", errno, strerror(errno));
    }
    ::close(wfd);
    return writen;
}

ssize_t File::appendWriteIn(const char* buf, size_t len) {
    if(NULL == buf || len <= 0 || strlen(filename_) == 0) { 
        return FILE_ARGS_INVALID;
    }

    if(!validFd(fd_)) {
        fd_ = ::open(filename_, O_WRONLY|O_CREAT|O_APPEND);
    }
    ssize_t writen = ::write(fd_, buf, len);
    if(writen != -1 && writen < len) { ::fsync(fd_); }
    if(-1 == writen) {
        INFO_LOG("continuousReadOut error: %d, %s", errno, strerror(errno));
    }
    return writen;
}

int File::send2Socket(int sockfd, size_t& sendn, off_t& offset, size_t count) {
    if(!validFd(sockfd)) { return FILE_FD_INVALID; }
    if(offset >= getFileSize()) { reset(); return FILE_SENDFILE_DONE; }
    if(offset + count > getFileSize()) { count = getFileSize() - offset; }

    if(!testExist() || !testRead()) { return FILE_READ_ACCESS_DENY; }
    if(!validFd(fd_)) { fd_ = ::open(filename_, O_RDONLY); }

    sendn = 0;
    int nonblock = (fcntl(sockfd, F_GETFL) & O_NONBLOCK);
    off_t old_off = offset;

    ssize_t n = 0;
    ssize_t left = count;
    //NonBlocking socket fd
    if(nonblock) {
        while (left > 0 &&  (n = sendfile(sockfd, fd_, &offset, left)) < left) { 
            if( n < 0 ) {
                if(errno == EAGAIN) { 
                    sendn = offset - old_off;
                    return FILE_SENDFILE_RECALL;
                }
                else { return FILE_SENDFILE_ERROR; }
            }
            offset += n;
            left -= n;
        }

        sendn = offset - old_off;
        //Send file done, need seek fd_ to beginning
        lseek(fd_, 0, SEEK_SET);
        reset();
        return FILE_SENDFILE_DONE;
    }
    //Blocking IO
    else {
        if(left == 0) {
            sendn = 0;
            lseek(fd_, 0, SEEK_SET);
            reset();
            return FILE_SENDFILE_DONE;
        }
        n = sendfile(sockfd, fd_, &offset, left); 
        offset += n;
        if(n == -1) {
            return FILE_SENDFILE_ERROR;
        }
        else if( n < left) {
            sendn = offset - old_off;
            return FILE_SENDFILE_RECALL;
        }
        else if(n == left) {
            sendn = offset - old_off;
            //Send file done, need seek fd_ to beginning
            lseek(fd_, 0, SEEK_SET);
            reset();
            return FILE_SENDFILE_DONE;
        }

    }
    return FILE_SENDFILE_ERROR;
}

int File::send2Socket(int sockfd, size_t& sendn) {
    if(!validFd(sockfd)) {
        return FILE_FD_INVALID;
    }

    if(!testExist() || !testRead()) { return FILE_READ_ACCESS_DENY; }
    if(!validFd(fd_)) {
        fd_ = ::open(filename_, O_RDONLY);
    }
    else {
        // not send any data before, need seek to beginning in case
        if(cur_ == 0) {
            if(lseek(fd_, 0, SEEK_SET) == -1) {
                return FILE_FD_INVALID;
            }
        }
    }

    sendn = 0;
    /*
       int block = 0;
       this->setBlockingIO(block);
       */

    int nonblock = (fcntl(sockfd, F_GETFL) & O_NONBLOCK);

    ssize_t n = 0;
    //update file size, in case the file has changed before recall this function
    size_ = getFileSize();
    ssize_t left = size_ - cur_;
    off_t old_off = cur_;

    //NonBlocking socket fd
    if(nonblock) {
        while (left > 0 &&  (n = sendfile(sockfd, fd_, NULL, left)) < left) { 
            if( n <  0 ) {
                if(errno == EAGAIN) {
                    cur_ += n;
                    sendn =  cur_ - old_off;
                    return FILE_SENDFILE_RECALL;
                }
                else {
                    return FILE_SENDFILE_ERROR; 
                }
            }
            left -= n;
        }

        sendn =  cur_ - old_off;
        cur_ = 0;
        lseek(fd_, 0, SEEK_SET);
        reset();
        return FILE_SENDFILE_DONE;
    }
    //Blocking IO
    else {
        if(left == 0) {
            sendn = 0;
            reset();
            return FILE_SENDFILE_DONE;
        }
        n = sendfile(sockfd, fd_, &cur_, left); 
        if(n == -1) {
            return FILE_SENDFILE_ERROR;
        }
        else if( n < left) {
            sendn = cur_ - old_off;
            return FILE_SENDFILE_RECALL;
        }
        else if(n == left) {
            sendn = cur_ - old_off;
            //Send file done, need seek fd_ to beginning
            cur_ = 0;
            lseek(fd_, 0, SEEK_SET);
            reset();
            return FILE_SENDFILE_DONE;
        }

    }
    return FILE_SENDFILE_ERROR;
}

void File::setFilename(const char* filename) {
    this->reset();
    if(nullptr == filename) { return; }
    bzero(filename_, sizeof(filename_));
    memcpy(filename_, filename, strlen(filename));
}

File::~File() {
    if(validFd(fd_)) {
        ::close(fd_);
    }
}

ssize_t File::getFileSize() {
    if(strlen(filename_) == 0) { return 0; }
    struct stat st;
    if(::stat(filename_,  &st) != 0) {
    
        printf("get file: %s size failed, errno: %d, %s", filename_, errno, strerror(errno));
        return -1; }
    return st.st_size;
}

}//namespace tigerso::core
