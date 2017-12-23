#include <unistd.h>
#include "core/Thread.h"
#include "core/CurrentThread.h"


namespace tigerso {

namespace CurrentThread {

   __thread int cacheTid_ = 0;
   __thread char tidString_[32] = {0};
   __thread size_t tidStrLength = 0;
   __thread const char* threadName_ = "unknow thread";

   pid_t tid() {
        if(__builtin_expect(cacheTid_== 0, 0)) {
            cacheTid_ = static_cast<pid_t>(::syscall(SYS_gettid));
            snprintf(tidString_, sizeof tidString_, "%d ", cacheTid_);
        }
        return cacheTid_;
   }

}

class ThreadData {
    typedef Thread::ThreadFunc ThreadFunc;
    ThreadFunc func_;
    std::string name_;
    std::weak_ptr<pid_t> wkTid_;

public:
    ThreadData(const ThreadFunc& func,
               const std::string& name,
               const std::shared_ptr<pid_t>& tid)
      : func_(func),
        name_(name),
        wkTid_(tid) {
    }

    void runInThread() {
        pid_t tid = CurrentThread::tid();
        std::shared_ptr<pid_t> ptid = wkTid_.lock();
        if(ptid){
            *ptid = tid;
            ptid.reset();
        }

        CurrentThread::threadName_ = name_.c_str();
        func_();
        CurrentThread::threadName_ = "finished";
    }
};

namespace initmain__ {
// before main, register atFork func
// outer sider should not access this

void afterFork() {
    //clear cachtid and thread name after fork
    CurrentThread::cacheTid_ = 0;
    //it must child mian thread
    CurrentThread::threadName_ = "main";
    //set main thread params
    CurrentThread::tid();
}

class ThreadInit {
public:
    ThreadInit() {
        CurrentThread::threadName_ = "main";
        CurrentThread::tid();
        pthread_atfork(NULL, NULL, afterFork);
    }
};

ThreadInit init;
}

void* startThread(void* obj) {
    ThreadData* data = static_cast<ThreadData*>(obj);
    data->runInThread();
    delete data;
    return nullptr;
}

std::atomic<int> Thread::numCreated_ (0);

Thread::Thread(const ThreadFunc& func, const std::string& name)
    : func_(func),
      name_(name),
      tid_(new pid_t(0)){
    setDefaultName();
}

void Thread::setDefaultName() {
    int num = ++numCreated_;
    if (name_.empty()) {
        char buf[32];
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}

void Thread::start() {
    start_ = true;
    //need take control of thread data live period
    ThreadData* data = new ThreadData(func_, name_, tid_);
    if(pthread_create(&threadId_, NULL, &startThread, static_cast<void*>(data))) {
        start_ = false;
        delete data;
    }
}

int Thread::join() {
    if(start_ && !joined_) {
        return pthread_join(threadId_, NULL);
    }
    return 1;
}

Thread::~Thread() {
    numCreated_ --;
}

}

