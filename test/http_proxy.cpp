#include "http/HttpProxy.h"
#include "core/ConfigParser.h"
#include "core/Logging.h"

tigerso::http::HttpProxyLoop* g_httpproxy = nullptr;
tigerso::core::ConfigParser* g_configini = nullptr;
tigerso::core::Logging* g_logging = nullptr;

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
        g_httpproxy = new tigerso::http::HttpProxyLoop("127.0.0.1", "8080");
    }
    
    if(g_configini == nullptr) {
        g_configini = tigerso::core::ConfigParser::getInstance();
    }

    if(g_logging == nullptr) {
        g_logging = tigerso::core::Logging::getInstance();
    }

    g_configini->setConfigPath("../etc/tigerso.ini");
    g_logging->setLogPath("./log");
    g_logging->setLevel("enable");

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
