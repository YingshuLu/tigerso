#ifndef TS_CORE_SYSUTIL_H_
#define TS_CORE_SYSUTIL_H_

#include <iostream>
#include <memory>
#include <string>
#include <sys/mman.h>
#include <sys/signal.h>
#include <semaphore.h>
#include "core/tigerso.h"
#include "core/BaseClass.h"

namespace tigerso {

using std::string;
using std::shared_ptr;

typedef void (signal_func)(int);
const int MC_LOCK_PRIVATE = 0;
const int MC_LOCK_SHARE_PROCESS = 1;
const string DEFAULT_SHM_MUTEX_FILENAME = "TIGERSOSHMMUTEX";

class SysUtil {
public:
    static string get_work_path();
    static string getFormatTime(const string&);
    static int make_dirtree(const string& );
    static int remove_file(const string& );
    static void* create_process_shared_memory(const string&, size_t);
    static int destroy_process_shared_memory(const string&, void*, size_t);
    static bool validate_filename(const string&);
    //signal process
    static signal_func *set_signal_handler(int signo, signal_func* func);
    static int daemon_start();

    void toLower(std::string& src, std::string& dst) { std::transform(src.begin(), src.end(), dst.begin(), ::tolower); }
    void toUpper(std::string& src, std::string& dst) { std::transform(src.begin(), src.end(), dst.begin(), ::toupper); }
    std::string trim(std::string&, const char f = ' ');
   
private:
    SysUtil();
    SysUtil(const SysUtil&);
    SysUtil& operator=(const SysUtil&);
};

class Lock: public nocopyable {
public:
    virtual int lock() = 0;
    virtual int try_lock() = 0;
    virtual int unlock() = 0;
protected:
    virtual int init() = 0;
    virtual int destroy() = 0;
    
};

class SharedMemory: public nocopyable {
public:
    SharedMemory():
        shm_prot(MAP_SHARED),
        shm_len(1024),
        shm_ptr(nullptr) {

        char rand_name [256] = {0};
        snprintf(rand_name, sizeof(rand_name), "randname_%d", (unsigned int) time(NULL));
        shm_name = string(rand_name);
        init();
    }

    SharedMemory(const string& name, const size_t size, const int prot = MAP_SHARED):
        shm_name(name),
        shm_prot(prot),
        shm_len(size),
        shm_ptr(nullptr) {

        init();
    }

    ~SharedMemory() {

        bool destroy = true;
        pid_t pid = getpid();

        if(pid != shm_pid) {
            destroy = false;
        }

        if(destroy) {
            this->destroy();
        }
    }

    void* get_shm_ptr() const;

protected:
    int init();
    int destroy();

private:
    string shm_name;
    pid_t shm_pid;
    int shm_prot;
    void* shm_ptr;
    size_t shm_len;
};

struct shm_mutex_t {
    pthread_mutex_t mutex;
    pthread_mutexattr_t mutexattr;
    char shm_name[1024];
    int refer_num;
};

class ShmMutex: public Lock {
public:

    ShmMutex();
    ShmMutex(const string&);
    ShmMutex(const ShmMutex&);
    
    ShmMutex operator=(const ShmMutex&); 

    int try_lock();
    int lock();
    int unlock();

    string get_shm_name() const;
    shm_mutex_t* get_shm_mutex() const;
    pid_t get_shm_pid() const;
    bool isLocked() { return locked_ ; }

    ~ShmMutex();

private:
    int init();
    int destroy();
    shm_mutex_t* mutex_ptr;
    string shm_name;
    pid_t shm_pid;
    pid_t locked_pid;
    bool locked_ = false;
};

class LockGuard {
public:
   explicit LockGuard(Lock& Lock)
        :Lock_obj(Lock) {
        Lock_obj.lock();
    }

    ~LockGuard() {
        Lock_obj.unlock();
    }

private:
    Lock& Lock_obj;
};

class LockTryGuard {
public:
    explicit LockTryGuard(Lock& Lock)
        :Lock_obj(Lock), locked(false) {
        locked = Lock_obj.try_lock() == 0? true: false;
    }

    ~LockTryGuard() {

    //  std::cout << "Try lock deconstruction" <<std::endl;
        if (locked) {
            Lock_obj.unlock();
        }
    }

    bool isLocked() const {
        return locked;
    }

private:
    Lock& Lock_obj;
    bool locked;
};

} // namespace tigerso::core
#endif
