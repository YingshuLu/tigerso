#include "http/HttpProxy.h"
#include "core/ConfigParser.h"
#include "core/Logging.h"
#include <string>

tigerso::HttpProxyLoop* g_httpproxy = nullptr;
tigerso::ConfigParser* g_configini = nullptr;
tigerso::Logging* g_logging = nullptr;

int initProxy();
int startProxy();
int destoryProxy();

int main() {
    initProxy();
    startProxy();
    destoryProxy();
    return 0;
}

int initProxy() {
    if(g_httpproxy == nullptr) {
        g_httpproxy = new tigerso::HttpProxyLoop("127.0.0.1", "8080");
    }
    
    if(g_configini == nullptr) {
        g_configini = tigerso::ConfigParser::getInstance();
    }

    if(g_logging == nullptr) {
        g_logging = tigerso::Logging::getInstance();
    }

    g_configini->setConfigPath("../etc/tigerso.ini");
    const std::string& logpath = g_configini->getValueByKey("log", "path");
    const std::string& loglevel = g_configini->getValueByKey("log", "verbose");
   
    g_logging->setLogPath(logpath);
    g_logging->setLevel(loglevel);

    return 0;
}

int startProxy() {
    g_httpproxy->initListenConnection();
    g_httpproxy->startLoop();
    return 0;
}

int destoryProxy() {
    delete g_httpproxy;
    return 0;
}
