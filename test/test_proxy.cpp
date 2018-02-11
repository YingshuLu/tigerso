#include "net/EventsLoop.h"
#include "net/Acceptor.h"
#include "net/ConnectionFactory.h"
#include "dns/DNSCache.h"
#include "dns/DNSResolver.h"
#include "core/ConfigParser.h"
#include "core/Logging.h"
#include "ssl/SSLContext.h"
#include "net/SocketUtil.h"

using namespace tigerso;

std::shared_ptr<EventsLoop> eloop = std::shared_ptr<EventsLoop>(new EventsLoop);
std::shared_ptr<ConnectionFactory> connfactptr = std::shared_ptr<ConnectionFactory>(new ConnectionFactory(eloop));
tigerso::ConfigParser* g_configini = nullptr;
tigerso::Logging* g_logging = nullptr;

int main() {


    if(g_configini == nullptr) {
        g_configini = tigerso::ConfigParser::getInstance();
    }

    if(g_logging == nullptr) {
        g_logging = tigerso::Logging::getInstance();
    }

    g_configini->setConfigPath("../etc/tigerso.ini");
    const std::string& logpath = g_configini->getValueByKey("log", "path");
    const std::string& loglevel = g_configini->getValueByKey("log", "verbose");

    const std::string& root = g_configini->getValueByKey("http", "root");
   
    g_logging->setLogPath(logpath);
    g_logging->setLevel(loglevel);

    DNSCache::init();
    OpensslInitializer sslIniter;
    const std::string value = g_configini->getValueByKey("dns", "primary");
    if(!value.empty() && SocketUtil::ValidateAddr(value)) {
        DNSResolver::setPrimaryAddr(value);
    }

    DNSResolver::setEventRegisterCallback(std::bind(&EventsLoop::registerChannel, eloop, std::placeholders::_1));

    std::shared_ptr<Acceptor> acptptr = std::shared_ptr<Acceptor>(new Acceptor("127.0.0.1", 8080, HTTP_PROXY_CONNECTION));
    acptptr->addIntoFactory(connfactptr);
    
    eloop->loop();
    return 0;
}
