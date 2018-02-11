#include <iostream>
#include "thread/ThreadPool.h"
#include "thread/ThreadMutex.h"
#include "core/SysUtil.h"

using namespace std;
using namespace tigerso;

int aa = 0;
void test() {
    cout << "aa = " << ++aa <<endl;
}

int main() {

    ThreadPool tpool("TestThreadPool");
    std::cout << "main thread " << CurrentThread::tid() <<std::endl;
    tpool.start(1);
    tpool.run(&test);
    tpool.run(&test);
    tpool.run(&test);
    tpool.run(&test);
    tpool.stop();
    return 0;
    
}
