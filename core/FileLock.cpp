#include "core/tigerso.h"
#include "core/FileLock.h"

namespace tigerso {
    
FileLock::FileLock(const std::string& filename, const std::string& content): filename_(filename), content_(content) {
    init();
}

int FileLock::lock() {
    if(lock_cmd(F_WRLCK) < 0) {
        return LOCK_FAILURE;
    }
    locked_ = true;
    return LOCK_SUCCESS;
}

int FileLock::try_lock() {
    return this->lock();
}

int FileLock::unlock() {
    if(lock_cmd(F_UNLCK) < 0) {
        return LOCK_FAILURE;
    }

    locked_ = false;
    return LOCK_SUCCESS;
}

int FileLock::lock_cmd(int cmd) {
    struct flock lk;
    lk.l_type = cmd;
    lk.l_start = 0;
    lk.l_whence = SEEK_SET;
    lk.l_len = 0;
    return fcntl(fd_, F_SETLK, &lk);
}

/*
int FileLock::getFileContent(char* buf, size_t len) {
    if(nullptr != buf && len > 0 ) {
      if(!content_.empty()) {
          memcpy(buf, content_.c_str(), content_.size());
          return 0;
      }

      if(locked_) {
            ssize_t sz = read(fd_, buf, len);
            return sz;
        }
      else if(this->lock() == LOCK_SUCCESS) {
            ssize_t sz = read(fd_, buf, len);
            this->unlock();
            return sz;
        }
    }
    return -1;
}
*/

FileLock::~FileLock() {
    this->destroy();
}

int FileLock::init() {
    if ((fd_ = open(filename_.c_str(), O_RDWR | O_CREAT | O_TRUNC )) < 0) {
       return -1; 
    }

    nonBlocking(fd_);
    size_t len = content_.size();
    if(locked_) {
        if(len != 0) {
            // need error process
            write(fd_, content_.c_str(), len);
        }
    }
    else if(this->lock() == LOCK_SUCCESS) {
        if(len != 0) {
            // need error process
            write(fd_, content_.c_str(), len);
            this->unlock();
        }
    }

    //set non-blocking IO
    return 0;
}

int FileLock::destroy() {
    if(locked_) {
        this->unlock();
    }
    close(fd_);
    return unlink(filename_.c_str());
}

}
