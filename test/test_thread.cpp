#include <iostream>
#include "core/ThreadPool.h"
#include "core/ThreadMutex.h"
#include "core/SysUtil.h"

using namespace std;
using namespace tigerso::core;

int aa = 0;
void test() {
    cout << "aa = " << aa <<endl;
}

int main() {

    ThreadPool tpool("TestThreadPool");
    std::cout << "tpoll created success" << std::endl;
    tpool.start(4);
    tpool.run(&test);
    tpool.run(&test);
    std::cout << "stop all thread, need get mutex" << std::endl;
    tpool.stop();
    return 0;
    
}
