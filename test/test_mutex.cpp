#include <iostream>
#include <string>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "core/SysUtil.h"
#include "core/Logging.h"
#include "core/FileLock.h"
#include "core/tigerso.h"

using namespace std;
using namespace tigerso::core;

struct share_count {

    share_count():count(0), pid(-1) {}

    int count;
    pid_t pid;
};

share_count* c_ptr = nullptr;
FileLock* sem = nullptr;
Logging* log_ptr = nullptr;

void print_share_count() {

    if (c_ptr != nullptr) {
        cout << ">>>> share count info <<<<" << endl;
        cout <<" pid: " << c_ptr->pid << endl;
        cout <<" count: " << c_ptr->count << endl;
        cout << ">>>> share count end \n<<<<" << endl;
    }
}

int child_start() {

    cout << "------->>> child ["<< getpid() << "] start <<<--------" << endl;
    int cnt = 4;
    while ((cnt--) > 0) {

        LockTryGuard trylock(*sem);
        if (!trylock.isLocked()) { 
            cout << "############# ("<<cnt<<") child [" << getpid() << "] lock failed, sleep 7s" << endl;
            sleep(7);
            continue;
        }
        else {
            
            cout << "############## (" <<cnt << ") child [" << getpid() << "] locked, go die" << endl;
            /*
            char buf[512] = {0};
            sem->getFileContent(buf, 512);
            cout << "File Lock content: " << buf << endl;
            FileLock fl("./tigerso.lock");
            cout << "new file lock, content: " << fl.getFileContent(buf, 25) << endl;
            */
            if (c_ptr != nullptr) {
                c_ptr->count += 1;
                c_ptr->pid = getpid();

                print_share_count();
                sleep(4);
            }
            break;
        }
    }
    cout << "------->>> child ["<< getpid() << "] end <<<--------\n" << endl;
}

int main(int argc, char* argv[]) {

    log_ptr = Logging::getInstance();
    log_ptr->setLogPath("/tmp");
    log_ptr->setLogPath("enable");

    char buf[25] = {0};
    
    pid_t mainpid = getpid();
    
    snprintf(buf, 25,"%d", mainpid);
    cout << "lock content to write: " << buf <<endl;
    sem = new FileLock("./tigerso.lock",buf);
    SharedMemory shm("count", sizeof(share_count));

    c_ptr = (share_count*) shm.get_shm_ptr();

    int child_num = 2;
    
    if(argc >= 2) {
        char* num_str = (char*)argv[1];
        int num = atoi(num_str);
        if (num > 0) {
            child_num = num;
        }
    }
    cout << "------->>> parent ["<< getpid() << "] start <<<--------" << endl;

    for(int i = 0; i < child_num; i++) {

        int ret = fork();

        if(ret < 0) {
            cout << "fork failed" << endl;
            continue;
        }
        //child start
        else if(ret == 0) {

            child_start();
            return 0;
        }
        else {

            cout << "parent create child " << ret << endl; 
        }
    }

    int n = 0;
    int stat;
    pid_t child_pid = -1;

    while ( n < child_num) {
        child_pid = wait(&stat);
        cout << "parent wait child "<< child_pid << endl;
        n++;
    }

    delete sem;
    cout << "------->>> parent ["<< getpid() << "] end <<<--------\n" << endl;
    return 0;
}
