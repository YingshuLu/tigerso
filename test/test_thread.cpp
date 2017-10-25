#include <iostream>
#include "core/ThreadPool.h"
#include "core/ThreadMutex.h"
#include "core/SysUtil.h"

using namespace tigerso::core;

int main() {

    ThreadPool tpool("TestThreadPool");
    std::cout << "tpoll created success" << std::endl;
    tpool.start(4);
    std::cout << "stop all thread, need get mutex" << std::endl;
    tpool.stop();
    return 0;
    
}
