#include <stdlib.h>
#include <string.h>
#include <execinfo.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <memory>
#include <set>
#include "core/BaseClass.h"
#include "core/SysUtil.h"
#include "core/File.h"
#include "core/ConfigParser.h"
#include "core/Logging.h"
#include "core/DaemonBase.h"
#include "net/Socket.h"
#include "ssl/SSLContext.h"
#include "dns/DNSResolver.h"

namespace tigerso {
    
#define __DAEMON_NAME "Damemon-base"
#define __BACKTRACE_NUM 32

struct SignalHandleSet {
    int signo;
    void(*handle)(int);
};

void __signalHandle(int);
static const SignalHandleSet DaemonSignals[] = {
    { SIGPIPE, SIG_IGN },
    { SIGCHLD, __signalHandle },
    { SIGSEGV, __signalHandle },
    { SIGTERM, __signalHandle },
    { SIGUSR1, __signalHandle }
};

typedef enum daemon_role_s {
    DAEMON_PARENT,
    DAEMON_CHILD
} DaemonRole;

static DaemonRole __daemon_role = DAEMON_PARENT;
static bool __parent_loop = true;
static bool __child_loop = true;
static bool __reload = false;
static std::set<pid_t> __childrens;

void __storeChildPid(const pid_t pid) {
    if(__daemon_role == DAEMON_PARENT) { __childrens.insert(pid); }
    return;
}

void __eraseChildPid(const pid_t pid) {
    if(__daemon_role == DAEMON_PARENT) { __childrens.erase(pid); }
    return;
}

void coreDumpPrints() {
    void* buffer[__BACKTRACE_NUM];
    int nptrs = backtrace(buffer, __BACKTRACE_NUM);
    INFO_LOG("backtrace() returned %d addresses", nptrs);
    char** strings = backtrace_symbols(buffer, nptrs);
    if(nullptr != strings) {
        for(int i = 0; i < nptrs; i++) { INFO_LOG("%s", strings[i]); }
    }
    free(strings);
    return;
}

void __signalHandle(int signo) {
    INFO_LOG("catch signal: %s(%d)", strsignal(signo), signo);
    switch(signo) {
        case SIGSEGV:
            coreDumpPrints();
            /*recover defalut handler*/
            ::signal(signo, SIG_DFL);
            if(__daemon_role == DAEMON_PARENT) {
                /*parent coredump, should kill her all children*/
                ::signal(SIGCHLD, SIG_IGN);
                DaemonBase::killAllChildren();
            }

            /*resend signo*/
            ::raise(signo);
            break;

        case SIGTERM:
            __daemon_role == DAEMON_PARENT? __parent_loop = false : __child_loop = false;
            break;

        case SIGCHLD:
                if(__daemon_role == DAEMON_PARENT && __parent_loop) {
                    pid_t childpid = 0;
                    int wstatus;
                    if((childpid = waitpid(-1, &wstatus, WNOHANG | WUNTRACED)) > 0) {
                        __eraseChildPid(childpid);
                    }
                }
            break;

        case SIGUSR1:
            INFO_LOG("reload now...");
            __reload = true;
            break;
        }
    return;
}

DaemonBase::DaemonBase(const std::string& name) {
    configPtr_ = ConfigParser::getInstance();
    configPtr_->setConfigPath(__CONFIG_FILE);
    adjustPidFileName(name);
}

DaemonBase::~DaemonBase() {
    if(::getpid() == parentPid_) { unlinkPidFile(); }
    destory();
}

int DaemonBase::start() {
    if(checkOldDaemon() != 0) { 
        printf("Daemon is running....\nexit now!\n");
        exit(EXIT_FAILURE);
    }

    daemonlize();

    INFO_LOG("write daemon pid file");
    writePidFile();

    initilize();
    return parentStart();
}
    
int DaemonBase::refresh() {
    __reload = false;
    configPtr_->reload();
    return loadConfigParams();
}

int DaemonBase::daemonInit() {
    return 0;
}

int DaemonBase::processInit() {
    return 0;
}

int DaemonBase::processStart() {
    INFO_LOG("process %d start", getpid());
    while(__child_loop) {}
    return 0;
}

int DaemonBase::processStop() {
    return 0;
}

int DaemonBase::childStart() {
    childInit();
    return processStart();
}

int DaemonBase::childStop() {
    return processStop();
}

int DaemonBase::childInit() {
    //init openssl
    INFO_LOG("Initilize OPENSSL lib");
    _OPENSSL_::init();
    return processInit();
}

int DaemonBase::stop() {
    if(DAEMON_PARENT == __daemon_role) { 
        killAllChildren();
        int wstatus;
        while (waitpid(-1, &wstatus, WNOHANG | WUNTRACED) > 0) {}
        __parent_loop = false;
    }
    else if(DAEMON_CHILD == __daemon_role) { 
        childStop();
        __child_loop = false;
    }
    
    return 0;
}

int DaemonBase::initilize() {
    loadConfigParams();

    //init shared dns cache
    DNSCache::init();
    INFO_LOG("Initilize DNS Cache");

    daemonInit();
    return 0;
}

int DaemonBase::destory() {
    _OPENSSL_::destory();
    return 0;
}

int DaemonBase::parentStart() {
    __daemon_role = DAEMON_PARENT;
    __parent_loop = true;

    pid_t pid  = -1;
    INFO_LOG("parent start");
    if(setupInterestedSignal() == -1) { return -1; }
    while(true) {
        if(__reload) { refresh(); }
        while(__childrens.size() < childMaxNum_) {
            pid = fork();
            if(pid < 0) {
                INFO_LOG("parent fork failed\n");
                stop();
                exit(EXIT_FAILURE);
            }
            else if(0 == pid) {
                __daemon_role = DAEMON_CHILD;
                __child_loop = true;
                childStart();
                return 0;
            }
            __storeChildPid(pid);
        }
        
        if(!__parent_loop) {
            INFO_LOG("Daemon stop...");
            stop();
            return -1;
        }
    }
    return 0;
}

int DaemonBase::killAllChildren() {
    for(auto& item : __childrens) { kill(item, SIGTERM); }
    return 0;
}

void DaemonBase::daemonlize() {
    pid_t pid = ::fork();
    if(-1 == pid) {
        printf("first fork error, errno: %d, %s", errno, strerror(errno));
        ::exit(EXIT_FAILURE);
    }
    else if(pid) { ::exit(EXIT_SUCCESS); }

    if(::setsid() < 0) {
        printf("setsid error, errno: %d, %s", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    pid = ::fork();
    if(-1 == pid) {
        printf("second fork error, errno: %d, %s", errno, strerror(errno));
        ::exit(EXIT_FAILURE);
    }
    else if(pid) { ::exit(EXIT_SUCCESS); }

    ::close(0); 
    ::open("/dev/null", O_RDWR);
    ::dup2(0, 1);
    ::dup2(0, 2);

    return;
}

int DaemonBase::adjustPidFileName(const std::string& name) {
    if(!name.empty()) { pidFilename_ = name; }
    pidFilename_ = "." + pidFilename_ + ".pid";
    return 0;
}

int DaemonBase::loadConfigParams() {
    //set child max number
    std::string value = configPtr_->getValueByKey("core", "childnum");
    if(!value.empty()) {
        childMaxNum_ = ::atoi(value.c_str());
        value.clear();
    }
    
    //set core file limit
    value = configPtr_->getValueByKey("core", "socketlimit");
    if(!value.empty()) {
        socketMaxNum_ = ::atoi(value.c_str());
        value.clear();
    }

    //set log path
    value = configPtr_->getValueByKey("log", "path");
    Logging* logptr = Logging::getInstance();
    logptr->setLogPath(value);
    
    //set log level
    value = configPtr_->getValueByKey("log", "verbose");
    logptr->setLevel(value);

    //set dns primary
    value = configPtr_->getValueByKey("dns", "primary");
    if(!value.empty()) { DNSResolver::setPrimaryAddr(value); }

    //set dns second
    value = configPtr_->getValueByKey("dns", "second");
    if(!value.empty()) { DNSResolver::setSecondAddr(value); }

    adjustResources();
    
    return 0;
}

int DaemonBase::writePidFile() {
    File pidFile(pidFilename_.c_str());
    parentPid_ = ::getpid();
    char pidstr[64] = {0};
    snprintf(pidstr, sizeof(pidstr), "%d", parentPid_);
    if(pidFile.writeIn(pidstr, strlen(pidstr)) < 0) {
        printf("failed to write in pid file: %s\n", pidFilename_.c_str());
        return -1;
    }
    return 0;
}

int DaemonBase::unlinkPidFile() {
    return File(pidFilename_.c_str()).unlink();
}
    
//ulimit(core, fd), umask
int DaemonBase::adjustResources() {
    //mask 644 
    umask(S_IXUSR|S_IWGRP|S_IXGRP|S_IWOTH|S_IXOTH);

    struct rlimit rlmt;
    //core
    rlmt.rlim_cur = rlmt.rlim_max = RLIM_INFINITY;
    setrlimit(RLIMIT_CORE, &rlmt);
    
    //fd
    rlmt.rlim_cur = rlmt.rlim_max = socketMaxNum_; 
    setrlimit(RLIMIT_NOFILE, &rlmt);

    return 0;
}

int DaemonBase::setupInterestedSignal() {
    struct sigaction sa;
    for(int i = 0; i < sizeof(DaemonSignals)/sizeof(SignalHandleSet); i++) {
        if(SysUtil::set_signal_handler(DaemonSignals[i].signo, DaemonSignals[i].handle) == SIG_ERR) {
            INFO_LOG("sigaction(%d) failed, errno:%d, %s", DaemonSignals[i], errno, strerror(errno));
            return -1;
        }
    }
    return 0;
}

int DaemonBase::checkOldDaemon() {
    File pidFile(pidFilename_.c_str());
    if(pidFile.testExist()) {
        INFO_LOG("daemon has been already running.\n");
        return -1;
    }
    return 0;
}

bool DaemonBase::isProcessNeedStop() { return __child_loop; }

bool DaemonBase::isProcessNeedReload() { return __reload; }

} //namespace tigerso
