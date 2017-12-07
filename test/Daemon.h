#include <stdlib.h>


class DaemonBase {

public:
    int start();
    int refresh();
    int stop();

private:
    int parentStart();
    int childStart();

    int writePidFile();
    int ulinkPidFile();

private:
    
    
};

