#include "core/ConfigParser.h"
#include "core/Logging.h"

using namespace tigerso;
using namespace tigerso::core;

ConfigParser* pconfig = ConfigParser::getInstance();
Logging* plog = Logging::getInstance();


int main() {

    pconfig->setConfigPath("tigerso.ini");
    plog->setLogPath("/tmp");
    plog->setLevel("enable");
    pconfig->getAllKey();

    DBG_LOG("hahah");
    return 0;
    
    
}
