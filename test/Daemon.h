#ifndef TS_CORE_DAEMONBASE_H_
#define TS_CORE_DAEMONBASE_H_

#include <stdlib.h>
#include <string.h>
#include <core/ConfigParser.h>
#include <net/Socket.h>
#include <string>

namespace tigerso {

void singalHandle(int signo);

class DaemonBase {
#define __CONFIG_FILE "../etc/tigerso.ini"
public:
    friend void singalHandle(int);
    /*
    friend class Singleton<DaemonBase>;
    friend class std::sharedPtr<DaemonBase>;
    */
    DaemonBase();
    ~DaemonBase();
    int start(const int port);
    int refresh();
    virtual int childStart();
    virtual int childStop();
    int stop();

private:
    int initilize();
    int destory();
    int parentStart();            
    int killAllChildren();
    void daemon();
    int adjustPidFileName(std::string& name);
    int loadConfigParams();
    int writePidFile();
    int unlinkPidFile();    
    void storeChildPid(const pid_t pid);
    void eraseChildPid(const pid_t pid);
    int adjustResources();
    void setupInterestedSingal();
    int checkOldDaemon();
    int createMasterListening(const std::string& ipaddr, const int port);
    DaemonBase(const DaemonBase&);
    DaemonBase& operator=(const DaemonBase&);

protected:
    bool isProcessNeedStop();
    bool isProcessNeedReload();
    int socketMaxNum_ = 1024;
    ConfigParser* configPtr_ = nullptr;
    SocketPtr master_;

private:
    pid_t parentPid_;
    int childMaxNum_ = 1;
    std::string pidFilename_ = "daemon-base";
};

} //namespace tigerso

#endif //TS_CORE_DAEMONBASE_H_
