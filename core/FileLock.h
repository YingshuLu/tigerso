#ifndef TS_CORE_FILELOCK_H_
#define TS_CORE_FILELOCK_H_

#include <unistd.h>
#include <fcntl.h>
#include <string>
#include "core/SysUtil.h"

#define LOCK_SUCCESS 0
#define LOCK_FAILURE -1

namespace tigerso::core {


class FileLock: public Lock {

public:
    FileLock(const std::string& filename, const std::string& conent = "");
    int lock();
    int try_lock();
    int unlock();
    //int getFileContent(char* buf, size_t len);
    ~FileLock();

protected:
    int init();
    int destroy();

private:
    int lock_cmd(int);

private:
    std::string filename_;
    std::string content_;
    bool locked_ = false;
    int fd_;
};

}

#endif

