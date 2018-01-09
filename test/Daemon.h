#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <execinfo.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <Memory>
#include <set>
#include <core/BaseClass.h>
#include <core/Sysutil.h>
#include <core/File.h>
#include <core/ConfigParser.h>
#include <core/Logging.h>
#include <net/Socket.h>
#include <ssl/SSLContext.h>
#include <dns/DNSResolver.h>

#define __DAEMON_NAME "Damemon-base"
#define __BACKTRACE_NUM 32

typedef struct signal_s {
    int signo;
    void (* handler)(int, siginfo_t*, void*);
    int flags;
} Signals;

typedef enum daemon_role_s {
    DAEMON_PARENT,
    DAEMON_CHILD
} DaemonRole;

const Signals DaemonSignals[] = {
{SIGCHLD, },
{},
{},
{},
{},
{},
{},
{},


};

class DaemonBase;
DaemonBase *__daemonptr = nullptr;

void coreDumpPrints() {
    char* buffer[__BACKTRACE_NUM];
    int nptrs = backtrace(buffer, __BACKTRACE_NUM);
    INFO_LOG("backtrace() returned %d addresses", nptrs);
    char** strings = backtrace_symbols(buffer, nptrs);
    if(nullptr == strings) { break; }
    for(int i = 0; i < nptrs; i++) { INFO_LOG("%s", strings[i]); }
    free(strings);
    return;
}

void singalHandle(int signo, siginfo_t* sinfo, void* data) {
    DaemonBase* daemonptr = reinterpret_cast<DaemonBase*>(data);
    switch(signo) {
        case SIGSEGV:
            coreDumpPrints();
            exit(EXIT_FAILURE);
            break;

        case SIGKILL:
        case SIGTERM:
            daemonptr->stop();
            break;

        case SIGCHLD:
                pid_t childpid = 0;
                int wstatus;
                if((childpid = waitpid(-1, &wstatus, WNOHANG | WUNTRACED)) > 0) {
                    if(DAEMON_PARENT == daemonptr->getRole()) { daemonptr->childrens_.erase(childpid); }
                }
            break;

        case SIGUSR1:
            INFO_LOG("catch signal: SIGUSR1, reload now...");
            daemonptr->refresh();
            break;
        }
    return;
}

class DaemonBase {

#define __CONFIG_FILE "../etc/tigerso.ini"
public:
    friend void singalHandle(int, siginfo_t*, void*);
    /*
    friend class Singleton<DaemonBase>;
    friend class std::sharedPtr<DaemonBase>;
    */
    DaemonBase() {
        configPtr_ = ConfigParser::getInstance();
        configPtr_->setConfigPath(__CONFIG_FILE);
        adjustPidFileName(__DAEMON_NAME);
    }

    DaemonBase(const DaemonBase&) = delete;
    DaemonBase& operator=(const DaemonBase&) = delete;
    
    ~DaemonBase() {
        if(::getpid() == parentPid_) { 
            destory();
            unlinkPidFile();
        }
    }

    int start(const int port) {
        if(checkOldDaemon() != 0) { 
            printf("Daemon is running....\nexit now!");
            exit(EXIT_FAILURE);
        }
        deamon();
        writePidFile();

        if(createMasterListening("127.0.0.1", port) != 0) {
            printf("Error: failed to create listening socket, %s:%d, reason: %s", host.c_str(), port, strerror(errno));
            exit(EXIT_FAILURE);
        }
        return parentStart();
    }

    
    int refresh() {
        configPtr_->reload();
        return loadConfigParams();
    }

    virtual int childStart();
    virtual int childStop();

    int getRole() { return role_; }
    int stop() {
        if(DAEMON_PARENT == role_) { 
            loop_ = false;
            int wstatus;
            while (waitpid(-1, &wstatus, WNOHANG | WUNTRACED) > 0) {}
            return;
        }
        else if(DAEMON_CHILD == role_) { return childStop(); }
        
        return 0;
    }

private:
    int initilize() {
        loadConfigParams();
        return 0;
    }

    int destory() {
        _OPENSSL::_destoryOpenssl();
        return 0;
    }

    int parentStart() {
        loadConfigParams();
        role_ = DAEMON_PARENT;

        pid_t pid  = -1;
        loop_ = true;
        while(true) {
            while(childrens_.size() < childMaxNum_) {
                pid = fork();
                if(pid < 0) {
                    DBG_LOG("parent fork failed\n");
                    exit(-1);
                }
                else if(0 == pid) {
                    setChildSignal();
                    role_ = DAEMON_CHILD;
                    return childStart();
                }
                childrens_.insert(pid);
            }
            setParentSignal();
            if(!loop_) { return -1; }
        }
        return 0;
    }

    int killAllChildren() {
        for(auto& item : childrens_) { kill(item, SIGTERM); }
        return 0;
    }

    void daemon() {
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

        int i;
        for (i = 0; i < 3; i++) { ::close(i); }
        int fd = ::open("/dev/null", O_RDWR);
        for (i = 0; i < 3; i++) { ::dup2(fd, i); }
        ::close(fd); 
    
        return;
    }

    int adjustPidFileName(std::string& name) {
        if(!name.empty()) { pidFilename_ = name; }
        pidFilename_ = "." + pidFilename_ + ".pid";
        return 0;
    }

    int loadConfigParams() {
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
        dnsPtr_ = DNSResolver::getInstance();
        value = configPtr_->getValueByKey("dns", "primary");
        if(!value.empty()) { dnsPtr_->setPrimaryAddr(value); }

        //set dns second
        value = configPtr_->getValueByKey("dns", "second");
        if(!value.empty()) { dnsPtr_->setSecondAddr(value); }

        //init openssl
        _OPENSSL_::_initOpenssl();

        adjustResources();
        
        return 0;
    }

    int writePidFile() {
        File pidFile(pidFilename_.c_str());
        parentPid_ = ::getpid();
        char pidstr[64] = {0};
        snprintf(pidstr, sizeof(pidstr), "%d", parentPid_);
        if(pidFile.writeIn(pidstr, strlen(pidstr)) < 0) {
            DBG_LOG("failed to write in pid file: %s\n", pidFilename_.c_str());
            return -1;
        }
        return 0;
    }

    int unlinkPidFile() {
        return File(pidFilename_.c_str()).unlink();
    }

private:
    
    //ulimit(core, fd), umask
    int adjustResources() {
        //mask 664 
        umask(S_IXUSR|S_IXGRP|S_IWOTH|S_IXOTH);

        struct rlimit rlmt;
        //core
        rlmt.rlim_cur = rlmt.rlim_max = RLIM_INFINITY;
        setrlimit(RLIMIT_CORE, &rlmt);
        
        //fd
        rlmt.rlim_cur = rlmt.rlim_max = socketMaxNum_; 
        setrlimit(RLIMIT_NOFILE, &rlmt);

        return 0;
    }

    int setParentSignal() {
        

    }

    int setChildSignal() {
        
    }

    int checkOldDaemon() {
        File pidFile(pidFilename_.c_str());
        if(pidFile.testExist()) {
            DBG_LOG("daemon has been already running.\n");
            return -1;
        }
        return 0;
    }

    int createMasterListening(const std::string& ipaddr, const int port) {
        char portstr[12] = {0};
        snprintf(portstr, sizeof(portstr), "%d", port);
        return SocketUtil::CreateListenSocket(ipaddr, port, true, master_);
    }

protected:
    int socketMaxNum_ = 1024;
    ConfigParser* configPtr_ = nullptr;
    DNSResolver* dnsPtr_= nullptr;

private:
    pid_t parentPid_;
    int childMaxNum_ = 1;
    bool loop_ = false;
    std::set<pid_t> childrens_;
    DaemonRole role_;

private:
    SocketPtr master_;
    std::string pidFilename_ = "daemon-base";
};

