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
#include "core/CurrentThread.h"
#include "core/Condition.h"
#include "core/ThreadMutex.h"
#include "core/Thread.h"

namespace tigerso::core {

class ThreadPool: public nocopyable {
public:
    typedef Thread::ThreadFunc Task;
    typedef std::shared_ptr<Thread> ThreadPtr;
    
    explicit ThreadPool(const std::string& name = "ThreadPool")
        : name_(name),
          notFull_(mutex_),
          notEmpty_(mutex_),
          running_(false){

    }
    
    void run(const Task task) {
        LockGuard lock(loopmutex_);
        printf("Lock run\n");
        while(isFull()) {
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
            printf("Create thread %s\n", id);
            threads_.push_back(std::make_shared<Thread>(std::bind(&ThreadPool::runInThread, this), id));
            threads_[i]->start();
        }
    }

    void runInThread() {
        while(running_) {
            Task task(take());
            printf("no task, thread loop\n");
            if(task) {
                task();
            }
        }
    }

    void stop() {
        {
        LockGuard lock(loopmutex_);
        printf("Lock stop\n");
        running_ = false;
        notEmpty_.notifyAll();
        }
        for(auto sp : threads_) {
           sp->join(); 
        }
    }

private:

    Task take() {
        LockGuard lock(loopmutex_);
        while(tasks_.empty()) {
            std::cout << "no task, looping" <<std::endl;
            notEmpty_.wait();
            std::cout << "not get task mutex" <<std::endl;
        }
        
        Task task = tasks_.front();
        tasks_.pop_front();
        
        notFull_.notify();
        return task;
    }

    bool isFull() const {
        if (loopmutex_.isLockedByCurrentThread()) {
            return tasks_.size() >= threadNum_;
        }
        LockGuard lock(loopmutex_);
        return tasks_.size() >= threadNum_;
    }

    mutable ThreadMutex loopmutex_;
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
