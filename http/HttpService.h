#ifndef TS_HTTP_HTTPSERVICE_H_
#define TS_HTTP_HTTPSERVICE_H_

#include <stdlib.h>
#include <string.h>
#include <string>
#include <memory>
#include <set>
#include "core/DaemonBase.h"
#include "core/Acceptor.h"
#include "core/ConnectionFactory.h"
#include "core/ConfigParser.h"
#include "core/Logging.h"
#include "dns/DNSResolver.h"

namespace tigerso {

class DaemonBase;
class HttpService: public DaemonBase {

#define HTTP_DAEMON_NAME "http-service"
public:
    HttpService(): DaemonBase(HTTP_DAEMON_NAME) {
    }

    ~HttpService(){}

protected:

    //init Accptor(s)
    int daemonInit() {
        ConfigParser* configptr = ConfigParser::getInstance();

        std::string value = configptr->getValueByHeader("core", "sendfile");
        if(strcasecmp("enable", value.c_str()) == 0) { HttpBodyFile::sendfile = true; }

        std::shared_ptr<Acceptor> acptptr;
        
        ServiceContext scontext;

        //Http web server
        value = configptr->getValueByHeader("http", "host");
        if(value.empty()) {
            INFO_LOG("http host is NULL in 'etc/tigerso.ini'");
            exit(EXIT_FAILURE);
        }

        scontext.host = value;

        value = configptr->getValueByHeader("http", "root");
        scontext.rootDir = value.empty()? "./" : value;

        value = configptr->getValueByHeader("http", "ssl");
        scontext.sslEnable = (value == "enable")? true : false;


        value = configptr->getValueByHeader("http", "port");
        int port = value.empty()? 80: atoi(value.c_str());

        acptptr = std::shared_ptr<Acceptor>(new Acceptor("127.0.0.1", port, HTTP_CONNECTION));
        acptptr->initService(scontext);
        acceptorSet_.insert(acptptr);
        
        return 0;
    }

    int processInit() {
        connfactptr_ = std::shared_ptr<ConnectionFactory>(new ConnectionFactory);

        //set DNS event register 
        DNSResolver::setEventRegisterCallback(std::bind(&ConnectionFactory::registerChannel, connfactptr_, std::place_holders::_1));

        for(auto& iter : acceptorSet_) {
            iter->addIntoFactory(connfactptr_);
        }
        
        return 0;
    }

    int processStart() {
        connfactptr_->start();
        return 0;
    }

    int processStop() {
        connfactptr_->stop();
    }

private:
    std::shared_ptr<ConnectionFactory> connfactptr_ = nullptr;
    std::set<std::shared_ptr<Acceptor>> acceptorSet_;

};

} // namespace tigerso

#endif
