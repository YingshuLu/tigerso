#include "net/EventsLoop.h"
#include "net/Acceptor.h"
#include "net/ConnectionFactory.h"
#include "core/ConfigParser.h"
#include "core/Logging.h"

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

    std::shared_ptr<Acceptor> acptptr = std::shared_ptr<Acceptor>(new Acceptor("127.0.0.1", 443, HTTP_CONNECTION));
    std::shared_ptr<Acceptor> acptptr1 = std::shared_ptr<Acceptor>(new Acceptor("127.0.0.1", 80, HTTP_CONNECTION));

    acptptr->addIntoFactory(connfactptr);
    acptptr1->addIntoFactory(connfactptr);

    ServiceContext scontext;
    scontext.host = "www.tigerso.com";
    scontext.rootDir = root.empty()? "./": root;
    scontext.sslEnabled = true;

    acptptr->initService(scontext);
    scontext.sslEnabled = false;
    acptptr1->initService(scontext);
    
    eloop->loop();
    return 0;
}
