#ifndef TS_UTIL_FILETYPEDETECTOR_H_
#define TS_UTIL_FILETYPEDETECTOR_H_

#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <magic.h>
#include <string>
#include "core/BaseClass.h"

namespace tigerso::util {

struct ScanFileType {
    const char* name;
    int typenum;
    const char* mime;
    const char* ext_list;
    const char* group;
};


//By default, only detect MIME-TYPE
//Using libmagic to perform this

class FileTypeDetector: public core::nocopyable {
public:
    FileTypeDetector() {}

    const char* detectFile(const std::string& file) { return detectFile(file.c_str()); }

    const char* detectFile(const char* filename) {
        if(cookie_ == NULL) { init(); }
        return magic_file(cookie_, filename);        
    }

    const char* detectFd(const int fd) {
        if(cookie_ == NULL) { init(); }
        return magic_descriptor(cookie_, fd);
    }

    const char* detectBuffer(const char* buffer, size_t len) {
        if(cookie_ == NULL) { init(); }
        return magic_buffer(cookie_, buffer, len);
    }

    ~FileTypeDetector() {
        if(cookie_ != NULL) { magic_close(cookie_); }
    }

private:
    int init(int flags = MAGIC_MIME_TYPE) {
        cookie_ = magic_open(flags);
        if(cookie_ == NULL) { return -1; }
        if(magic_load(cookie_, NULL) != 0) { return -1; }
        return 0;
    }

    magic_t cookie_ = NULL;
};

} //namespace tigerso::util
#endif
