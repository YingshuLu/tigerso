#ifndef TS_HTTP_HTTPSERVICE_H_
#define TS_HTTP_HTTPSERVICE_H_

#include <string>
#include <memory>
#include <set>

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
        std::string value = configptr->getValueByHeader("http", "sendfile");
        if(strcasecmp("enable", value.c_str()) == 0) { HttpBodyFile::sendfile = true; }

        std::shared_ptr<Acceptor> acptptr;
        

    }

    int processInit() {
        connfactptr_ = std::shared_ptr<ConnectionFactory>(new ConnectionFactory);
        for(auto& iter : acceptorSet_) {
            iter->addIntoFactory(connfactptr_);
        }
        
        return 0;
    }

    int processStart() {
        connfactptr_->start();
    }

    int processStop() {
        connfactptr_->stop();
    }

    


private:
    std::shared_ptr<ConnectionFactory> connfactptr_ = nullptr;
    std::vector<std::shared_ptr<Acceptor>> acceptorSet_;

};

} // namespace tigerso

#endif
