#ifndef TS_CORE_PTHREAD_H_
#define TS_CORE_PTHREAD_H_

#include <stdio.h>
#include <pthread.h>
#include <string>
#include <atomic>
#include <functional>
#include <memory>
#include "core/CurrentThread.h"
#include "core/BaseClass.h"

namespace tigerso::core {

class Thread: public nocopyable {
public:
    typedef std::function<void()>  ThreadFunc;

    explicit Thread(const ThreadFunc& func, const std::string& name = "");

    ~Thread();

    void start();

    int join();
    
    bool started() const { return start_; }

    pid_t tid() const { return *tid_; }

    const std::string& name() { return name_; }

    static int numCreated() { return numCreated_.load(); }
    
private:
    void setDefaultName();

    ThreadFunc func_;
    bool start_ = false;
    bool joined_ = false;
    pthread_t threadId_ = 0;
    std::shared_ptr<pid_t> tid_;
    std::string name_="";

    static std::atomic<int> numCreated_;
};

} //namespace tigerso::core
#endif
