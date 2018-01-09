#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <regex>
#include <time.h>
#include "core/SysUtil.h"
#include "core/Logging.h"

namespace tigerso {

using std::string;
using std::cout;
using std::endl;
using std::regex;


string SysUtil::get_work_path()
{
    return core::WORKPATH;
}

string SysUtil::getFormatTime(const string& fmt){

    const size_t BUFSIZE = 1024;
    
    time_t tm = time(0);
    char tmpBuf[BUFSIZE] = {0};
    strftime(tmpBuf, BUFSIZE, fmt.c_str(), localtime(&tm));
    return tmpBuf;
}

int SysUtil::make_dirtree(const string& dirname)
{
    if(dirname.length() == 0)
    {
        return -1;
    }
    return 0;
}

int SysUtil::remove_file(const string& file)
{
    return 0;
}

void* SysUtil::create_process_shared_memory(const string& shm_name, size_t len, bool clear)
{
    
    if(shm_name == "" || shm_name.length() == 0 || len <= 0)
    {
        DBG_LOG("1. create error: %s", strerror(errno));
         return NULL;
    }

    //if /dev/shm/$(shm_name) existed, remove it
    string shm_path = shm_name;
    if(access(shm_path.c_str(), F_OK) == 0 && clear)
    {
        DBG_LOG("2. create error: %s", strerror(errno));
        if( unlink(shm_path.c_str())!=0)
        {
            DBG_LOG("2.1. create error: %s", strerror(errno));
            return NULL;
        }
    }
    DBG_LOG("2.2 create error: %s", strerror(errno));
    
    string shm_file =  shm_name;

    int fd = shm_open(shm_file.c_str(), O_RDWR|O_CREAT|O_EXCL, 0755);
     DBG_LOG("3. create error: %s, shm_file: %s, fd: %d", strerror(errno), shm_file.c_str(), fd);
    if ( fd < 0 ){ 
        INFO_LOG("shm_open failed, errno: %d, %s", errno, strerror(errno));
        return NULL;
    }
    
    ftruncate(fd, len);

    void* ptr = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

    if(clear) { ::bzero(ptr, len); }
    ::close(fd);

    DBG_LOG("4. create error: %s", strerror(errno));
    return ptr;
}

int SysUtil::destroy_process_shared_memory(const string& shm_name, void* ptr, size_t len)
{
    if(shm_name == "" || shm_name.length() == 0 || ptr == NULL || len <= 0)
    {
         INFO_LOG("params error");
         return -1;
    }

    DBG_LOG("destroy process shared memory: %s", shm_name.c_str());
    munmap(ptr, len);
    return shm_unlink(shm_name.c_str());
}

bool SysUtil::validate_filename(const string& filename)
{
    regex pattern ("[0-9a-zA-Z\\.\\-_]{1,128}");
    return regex_match(filename, pattern);
}

signal_func* SysUtil::set_signal_handler(int signo, signal_func* func) {

    struct sigaction act, oact;
    
    act.sa_handler = func;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    if(::sigaction(signo, &act, &oact) < 0) {
        return SIG_ERR;
    }

    return oact.sa_handler;
}

void* SharedMemory::get_shm_ptr() const {

    return shm_ptr;
}

int SharedMemory::init() {

    shm_pid = getpid();
    if(shm_len <= 0)
    {
        DBG_LOG("1. create error: %s", strerror(errno));
         return -1;
    }

    if(shm_name.empty() || shm_name.size() == 0) {

        shm_name = DEFAULT_SHM_MUTEX_FILENAME+SysUtil::getFormatTime("%H%M%S");
    }

    /*
    //if /dev/shm/$(shm_name) existed, remove it
    if(access(shm_name.c_str(), F_OK) == 0)
    {
      //  DBG_LOG("2. create error: %s", strerror(errno));
        if(unlink(shm_name.c_str())!=0)
        {
        //    DBG_LOG("2.1. create error: %s", strerror(errno));
            return -1;
        }
    }
   // DBG_LOG("2.2 create error: %s", strerror(errno));
    
    int fd = shm_open(shm_name.c_str(), O_RDWR|O_CREAT, 0755);
    // DBG_LOG("3. create error: %s, shm_file: %s, fd: %d", strerror(errno), shm_file.c_str(), fd);
    if ( fd < 0)
    {
        cout << "fd shm_open failed: "<< strerror(errno) << endl;
        return -1;
    }
    
    ftruncate(fd, shm_len);

    shm_ptr = mmap(NULL, shm_len, PROT_READ|PROT_WRITE, shm_prot, fd, 0);
    */

    //this->shm_name += SysUtil::getFormatTime("-%G%b%d%H%M%S");
    shm_unlink(shm_name.c_str()); 
    shm_ptr = SysUtil::create_process_shared_memory(shm_name, shm_len);
    DBG_LOG("shm_t shm name: %s, shm ptr: %ld", shm_name.c_str(), shm_ptr);

    //::close(fd);

    // DBG_LOG("4. create error: %s", strerror(errno));
    return 0;
}

int SharedMemory::destroy() {

    if(shm_name.empty() || shm_ptr == NULL || shm_len <= 0)
    {
         return -1;
    }

    DBG_LOG("destroy process shared memory: %s", shm_name);
    SysUtil::destroy_process_shared_memory(shm_name, shm_ptr, shm_len);
    return -1;
}

// ShmMutex
ShmMutex::ShmMutex()
{
    this->shm_name = "";
    this->shm_pid = getpid();
    this->mutex_ptr = NULL;
    init();
    if(mutex_ptr) {
       mutex_ptr->refer_num = 1;
    }
}

ShmMutex::ShmMutex(const string& shm_name)
{

    this->shm_name = shm_name;
    this->shm_pid = getpid();
    this->mutex_ptr == NULL;
    init();
    if(mutex_ptr) {
       mutex_ptr->refer_num = 1;
    }
}

ShmMutex::ShmMutex(const ShmMutex& mutex)
{
    this->shm_name = mutex.get_shm_name();
    this->shm_pid = getpid();
    this->mutex_ptr = mutex.get_shm_mutex(); 
    if(mutex_ptr) {
       mutex_ptr->refer_num += 1;
    }
}

ShmMutex ShmMutex::operator=(const ShmMutex& mutex)
{
    if(this == &mutex) {
        return *this;
    }

    this->shm_name = mutex.get_shm_name();
    this->shm_pid = getpid();
    this->mutex_ptr = mutex.get_shm_mutex(); 
    if(mutex_ptr) {
       mutex_ptr->refer_num += 1;
    }

    return *this;
}

ShmMutex::~ShmMutex()
{
    pid_t pid = getpid();

    if(locked_) { this->unlock(); }
    if(NULL != mutex_ptr) {
        //DBG_LOG("[Test in destroy] get mutex refer num: %d ", mutex_ptr->refer_num);
        mutex_ptr->refer_num --;
    }

    //if(pid == shm_pid && mutex_ptr != NULL && mutex_ptr->refer_num == 0)
    DBG_LOG("current pid = %d, parent pid = %d", getpid(), shm_pid);
    if(pid == shm_pid)
    {
        //DBG_LOG("shm mutex destroy mutex_ptr: %d", mutex_ptr);
        DBG_LOG("shm mutex: %s, shm mutex destroy mutex_ptr: %ld", shm_name.c_str(),  mutex_ptr);
        destroy();
        SysUtil::destroy_process_shared_memory(shm_name, (void*) mutex_ptr, sizeof(shm_mutex_t));
        std::string fn = "/dev/shm" + shm_name;
        remove(fn.c_str());
    }
}

string ShmMutex::get_shm_name() const
{
    return shm_name;
}

shm_mutex_t* ShmMutex::get_shm_mutex() const
{
    return mutex_ptr;
}

int ShmMutex::init()
{
    //McLog* mylog = McLog::getInstance();
    //mylog->setLogPath(LOGPATH);
    //mylog->setLevel("enable");
    if(mutex_ptr != NULL)
    {
        return 0;
    }
    
    if (shm_name.empty()) {
        this->shm_name = DEFAULT_SHM_MUTEX_FILENAME;
    }

    //this->shm_name += SysUtil::getFormatTime("-%G%b%d%H%M%S");
    shm_unlink(shm_name.c_str()); 
    size_t len = sizeof(shm_mutex_t);
    void* ptr = SysUtil::create_process_shared_memory(shm_name, len);

    DBG_LOG("Create mutex. errno: %d, init error: %s", errno, strerror(errno));
    if(ptr == NULL)
    {
        DBG_LOG("ptr == NULL, init error: %s", strerror(errno));
        return -1;
    }

    mutex_ptr = (shm_mutex_t*) ptr;

    bzero(mutex_ptr, len);

    memcpy(mutex_ptr->shm_name, this->shm_name.c_str(), this->shm_name.size());

    int ret = 0;

    ret = pthread_mutexattr_init(&(mutex_ptr->mutexattr));
    if (ret != 0) {
        DBG_LOG("mutexattr init failed, init error: %s", strerror(errno));
        return ret;
    }

    ret = pthread_mutexattr_setpshared(&(mutex_ptr->mutexattr), PTHREAD_PROCESS_SHARED);
    if (ret != 0) {
        DBG_LOG("mutexattr init failed, init error: %s", strerror(errno));
        return ret;
    }

    ret = pthread_mutex_init(&(mutex_ptr->mutex), &(mutex_ptr->mutexattr));
    if (ret != 0) {
        DBG_LOG("mutexattr init failed, init error: %s", strerror(errno));
        return ret;
    }
    return 0;
}

int ShmMutex::lock()
{
    if (mutex_ptr != NULL) {
        //DBG_LOG("mutex lock, mutex_ptr: %d", mutex_ptr);
        int ret = pthread_mutex_lock(&(mutex_ptr->mutex));
        if(ret != 0) {
            DBG_LOG("mutex lock failed, return code [%d] : %s", ret, strerror(errno));
            return ret;
        }
        locked_pid = getpid();
        locked_ = true;
        return ret;
    }
    return -1;
}

int ShmMutex::try_lock()
{
    if (mutex_ptr != NULL) {
        //DBG_LOG("mutex try lock, mutex_ptr: %d, mutex refer: %d", mutex_ptr, mutex_ptr->refer_num);
        int ret =  pthread_mutex_trylock(&(mutex_ptr->mutex));
        if(ret != 0) {
            DBG_LOG("mutex try lock failed, return code [%d] : %s", ret, strerror(errno));
            return ret;
        }
        locked_pid = getpid();
        locked_ = true;
        return ret;
    }
    return -1;
}

int ShmMutex::unlock()
{
    if(mutex_ptr != NULL)
    {
        //DBG_LOG("mutex unlock, mutex_ptr: %d, mutex refer: %d", mutex_ptr, mutex_ptr->refer_num);
        int ret = pthread_mutex_unlock(&(mutex_ptr->mutex));
        if(ret != 0) {
             std::cout<< "shm mutex unlock failed:"<<strerror(errno) << std::endl;
             DBG_LOG("mutex unlock failed, return code [%d] : %s", ret, strerror(errno));
        }
        else {
            locked_pid = 0;
            locked_ = false;
            DBG_LOG("shm mutex unlock success");
        }
        return ret;
    }
    return -1;
}

int ShmMutex::destroy()
{
    if(mutex_ptr != NULL)
    {
        //DBG_LOG("shm mutex destroy, destroy mutex_ptr: %d", mutex_ptr);
        int ret = pthread_mutex_destroy(&(mutex_ptr->mutex));
        if(ret != 0) {
            DBG_LOG("mutex destroy failed, return code [%d] : %s", ret, strerror(errno));
        }
        return ret;
    }
    return 0;
}

} // namespace tigerso::core
