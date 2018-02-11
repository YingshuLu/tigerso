#ifndef TS_CORE_THREADPOOL_H_
#define TS_CORE_THREADPOOL_H_

#include <stdio.h>
#include <iostream>
#include <string>
#include <deque>
#include <vector>
#include <memory>
#include "core/SysUtil.h"
#include "core/BaseClass.h"
#include "thread/CurrentThread.h"
#include "thread/Condition.h"
#include "thread/ThreadMutex.h"
#include "thread/Thread.h"

namespace tigerso {

class ThreadPool: public nocopyable {
public:
    typedef Thread::ThreadFunc Task;
    typedef std::shared_ptr<Thread> ThreadPtr;
    
    explicit ThreadPool(const std::string& name = "ThreadPool")
        : name_(name),
          notFull_(mutex_),
          notEmpty_(mutex_),
          running_(false) {

    }

    ~ThreadPool() {
        if(running_) {
            stop();
        }
    }
    
    void run(const Task task) {
        LockGuard lock(mutex_);
        printf("locked run by %d\n", mutex_.tid());
        while(isFull()) {
            printf("tasks is full, waiting\n");
            notFull_.wait();
        }
        
        tasks_.push_back(task);
        notEmpty_.notify();
        return;
    }

    void start(const int threadNum) {
        threadNum_ = threadNum;
        threads_.reserve(threadNum_);
        running_ = true;

        for (int i = 0; i < threadNum_; i++) {
            char id[32];
            snprintf(id, sizeof id, "%d", i+1);
            threads_.push_back(std::make_shared<Thread>(std::bind(&ThreadPool::runInThread, this), id));
            threads_[i]->start();
        }
    }

    void runInThread() {
        while(running_) {
            printf("thread %d running\n", CurrentThread::tid());
            Task task(take());
            if(task) {
                task();
            }
        }
    }

    void stop() {
        printf("stop all threads\n");

        {
        LockGuard Lock(mutex_);
        running_ = false;
        notEmpty_.notifyAll();
        }

        for(auto sp : threads_) {
           sp->join(); 
        }
    }

private:

    Task take() {
        LockGuard lock(mutex_);
        while(tasks_.empty() && running_) {
            std::cout << "tasks is empty, waiting" <<std::endl;
            notEmpty_.wait();
        }
        
        Task task = nullptr;
        if(!tasks_.empty()) {
            task = tasks_.front();
            tasks_.pop_front();
            notFull_.notify();
        }
        return task;
    }

    bool isFull() const {
        return tasks_.size() >= threadNum_;
    }

    mutable ThreadMutex mutex_;
    Condition notFull_;
    Condition notEmpty_;
    std::vector<ThreadPtr> threads_;
    std::deque<Task> tasks_;
    std::string name_;
    int threadNum_;
    std::atomic<bool> running_;
    bool notFull = true;
    bool notEmpty = false;
};


} //namespace tigerso::core


#endif
