#ifndef TS_CORE_CURRENTTHREAD_H_
#define TS_CORE_CURRENTTHREAD_H_

#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

namespace tigerso {

namespace CurrentThread {

   extern __thread int cacheTid_;
   extern __thread char tidString_[32];
   extern __thread size_t tidStrLength;
   extern __thread const char* threadName_;

   pid_t tid();

   inline bool isMainThread() { return CurrentThread::tid() == getpid(); }

   inline const char* tidString() { return tidString_; }

   inline const char* name() { return threadName_; }
}

}//namespace core


#endif
