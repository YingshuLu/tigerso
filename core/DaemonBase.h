#ifndef TS_CORE_DAEMONBASE_H_
#define TS_CORE_DAEMONBASE_H_

#include <stdlib.h>
#include <string.h>
#include <core/ConfigParser.h>
#include <net/Socket.h>
#include <string>

namespace tigerso {

class DaemonBase {
#define __CONFIG_FILE "../etc/tigerso.ini"
public:
    friend void __signalHandle(int);

    DaemonBase(const std::string& name);
    virtual ~DaemonBase();
    int start();
    int refresh();
    int stop();

private:
    int initilize();
    int destory();
    int parentStart();
    int childStart();
    int childStop();            
    int childInit();
    static int killAllChildren();
    void daemonlize();
    int adjustPidFileName(const std::string& name);
    int loadConfigParams();
    int writePidFile();
    int unlinkPidFile();    
    void storeChildPid(const pid_t pid);
    void eraseChildPid(const pid_t pid);
    int adjustResources();
    int setupInterestedSignal();
    int checkOldDaemon();
    DaemonBase(const DaemonBase&);
    DaemonBase& operator=(const DaemonBase&);

protected:
    virtual int daemonInit();
    virtual int processInit();
    virtual int processStart();
    virtual int processStop();
    bool isProcessNeedStop();
    bool isProcessNeedReload();
    int socketMaxNum_ = 1024;
    ConfigParser* configPtr_ = nullptr;

private:
    pid_t parentPid_;
    int childMaxNum_ = 1;
    std::string pidFilename_ = "daemon-base";
};

} //namespace tigerso

#endif //TS_CORE_DAEMONBASE_H_
