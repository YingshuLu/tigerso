#ifndef TS_CORE_THREADMUTEX_H_
#define TS_CORE_THREADMUTEX_H_

#include <sys/types.h>
#include <pthread.h>
#include <string>
#include "core/SysUtil.h"
#include "core/BaseClass.h"
#include "core/CurrentThread.h"

namespace tigerso {

class ThreadMutex: public Lock {
public:
    ThreadMutex()
        :holder_(0) {
        pthread_mutex_init(&mutex_, NULL);    
    }

    int lock() {
        int ret = pthread_mutex_lock(&mutex_);
        if(ret == 0) {
            assignHolder();
        }
        return ret;
    }

    int try_lock() {
        int ret = pthread_mutex_trylock(&mutex_);
        if (ret == 0) {
            assignHolder();
        }
        return ret;
    }

    int unlock() {
        int ret = pthread_mutex_unlock(&mutex_);
        if (ret == 0) {
            unassignHolder();
        }
        return ret;
    }
    
    bool isLockedByCurrentThread() const {
        return holder_ == core::CurrentThread::tid();
    }

    ~ThreadMutex() {
        pthread_mutex_destroy(&mutex_);
    }

private:
    friend class Condition;

    pthread_mutex_t* getThreadMutex() {
        return &mutex_;
    }

    int init() {
        return pthread_mutex_init(&mutex_, NULL);
    }

    int destroy() {
        return pthread_mutex_destroy(&mutex_);
    }

    void assignHolder() {
        holder_ = CurrentThread::tid();
    }

    void unassignHolder() {
        holder_ = 0;
    }
    
    class UnassignGuard {
    public:
        UnassignGuard(ThreadMutex& mutex)
        :owner_(mutex) {
            owner_.unassignHolder();
        }
        
        ~UnassignGuard() {
            owner_.assignHolder();
        }
    
    private:
        ThreadMutex& owner_;
    };

    pthread_mutex_t  mutex_;
    pid_t holder_ = 0;
};

} // namespace tigerso::core

#endif
