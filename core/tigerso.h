#ifndef TS_CORE_TIGERSO_H_
#define TS_CORE_TIGERSO_H_

#include <stdio.h>
#include <string.h>
#include <cctype>
#include <stdlib.h>
#include <libgen.h>
#include <ctype.h>
#include <errno.h>
#include <malloc.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdarg.h>
#include <string>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <assert.h>
#include <string>
#include <algorithm>

namespace tigerso::core {

//Project Marcos
const std::string  PROJECT     =  "tigerso";
const std::string  VERSION     =  "1.0.0";
const std::string  WORKPATH    =  "/usr/" + PROJECT;
const std::string  CONFIGPATH  =  WORKPATH + "/etc";
const std::string  LOGPATH     =  WORKPATH + "/log";
const std::string  CONFIGFILE  =  CONFIGPATH + "/" + PROJECT  +".ini";
const std::string  LOGNAME     =  "httpd";

} //namespace tscore
#endif // TS_CORE_TIGERSO_H_
