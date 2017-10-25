#ifndef TS_HTTP_HTTPPROXY_H_
#define TS_HTTP_HTTPPROXY_H_

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "core/BaseClass.h"
#include "net/Socket.h"
#include "net/SocketUtil.h"
#include "net/Buffer.h"
#include "net/EventsLoop.h"
#include "http/HttpMessage.h"
#include "http/HttpProxy.h"

namespace tigerso::http {

HttpProxy::HttpProxy(const SocketPtr& sockptr, EventsLoop& loop)
        : clientPtr_(sockptr),
          loop_(loop) 
{
        setReadCallback(clientPtr_, std::bind(&HttpProxy::recvEvent, *this));
        setWriteCallback(clientPtr_, std::bind(&HttpProxy::writeEvent, *this));
        setErrorCallback(clientPtr_,std::bind(&HttpProxy::errorEvent. *this));
        enableReadEvent(clientPtr_);
}

void HttpProxy::destroy() {
        clientPtr_ -> close();
        for(auto iter = hostsServerPtrs_; iter != hostsServerPtrs_.end(); iter ++) {
            (*iter)->close();
        }
}

int HttpProxy::errorEvent(SocketPtr& sockptr) {
       sockptr->close();
       if(sockptr->getRole() == SOCKET_ROLE_CLIENT) {
            destroy();
       }
       return EVENT_CALLBACK_BREAK;
}

int HttpProxy::recvEvent(SocketPtr& sockptr) {
       disableReadEvent(sockptr);
       if (sockptr->recvNIO() >= 0) {
            // client socket
            if (sockptr->getRole() == SOCKET_ROLE_CLIENT) {
                auto bufptr = sockptr->getInBufferPtr();
                HttpRequest request;
                parser_.parse(bufptr->toString(), request);
                cout<< " >> ----- " << request.getVersion() << "  " << request.getMethod() << "  " << request.getUrl() << " from client " <<sockptr->getStrAddr <<endl;
                string outstr = HttpResponse::NOT_FOUND;
                string domain = request.getValueByHeader("host");
                vector<string> IPs;
                SocketUtil::ResolveHost2IP(domain, IPs);
                cout << " >> + Domain: " << domain << endl;
                for(auto &item : IPs) {
                    cout << " >> - Host: " << item << endl;
                }

                auto outptr = sockptr->getOutBufferPtr();
                if(IPs.empty()) {
                    outptr->addData(outstr);
                    setWriteCallback(sockptr, std::bind(&HttpProxy::writeEvent, *this));
                    enableWriteEvent(sockptr);
                    if(!needClose(sockptr)) {
                        needCloseSocketPtrs_.push_back(sockptr);
                    }
                }
                else {
                    SocketPtr serverPtr = findServerPtr(domain);
                    if(!serverPtr->exist()) {
                        SocketUtil::Connect(*serverPtr, IPs[0], "80");
                        if(serverPtr->exist()) {
                            cout<< "socket = " << serverPtr->getSocket() << endl;
                            insertHostSockPtr(domain, serverPtr);
                        }
                    }
                    serverPtr->setOutBufferPtr(sockptr->getInBufferPtr());
                    sockptr->setOutBufferPtr(server->getInBufferPtr());
                    setWriteCallback(serverPtr, writeEvent);
                    enableWriteEvent(serverPtr);
            }
            else if(sockptr->getRole() == SOCKET_ROLE_SERVER) {
                auto bufptr = sockptr->getOutBufferPtr();
                HttpResponse response;
                parser_.parse(bufptr->toString(), response);
                enableWriteEvent(clientPtr_);
            }
       }

       if (needClose(sockptr)) {
            sockptr->close();
            return EVENT_CALLBACK_BREAK;
       }

       return EVENT_CALLBACK_CONTINUE;
}

int HttpProxy::writeEvent(SocketPtr& sockptr) {
        disableWriteEvent(sockptr);
        if(!SockUtil::TestConnect(*sockptr) {
            sockptr->close();
            if (sockptr->getRole() == SOCKET_ROLE_CLIENT) {
                destroy(); 
            }
            return EVENT_CALLBACK_BREAK;
        }

        if(sockptr->sendNIO() >=0) {
            auto bufptr = sockptr->getOutBufferPtr();
            if (bufptr->getReadableBytes() > 0) {
                enableWriteEvent(sockptr);
                return EVENT_CALLBACK_CONTINUE;
            } 
        }
        else {
            sockptr->close();
            if(sockptr->getRole() == SOCKET_ROLE_CLIENT) {
                destroy();
            }
        }
    return EVENT_CALLBACK_BREAK;
}

bool HttpProxy::needClose(SocketPtr& sockptr) {

    auto iter = std::find_if(needCloseSocketPtrs_.begin(), needCloseSocketPtrs_.end(), [&](SocketPtr& sp){ return *sockptr == *sp;  });

    return !(iter == needCloseSocketPtrs_.end());
}

SocketPtr HttpProxy::findServerPtr(const string& host) {
        //Based on host
        auto iter = hostsServerPtrs_.find(host);
        if(iter != hostsServerPtrs_.end()) {
            return iter->second;
        }

        return std::make_shared<Socket>();
}

void HttpProxy::insertHostSockPtr(std::string& host, SocketPtr& serverPtr) {

    if(findServerPtr(host)->exist() == false) {
        hostsServerPtrs_[host] = serverPtr;
    }
}

void HttpProxy::setReadCallback(SocketPtr& sockptr, const EventFunc& func) {
        auto cnptr = loop_.getChannel(sockptr);
        cnptr->setReadCallback(func); 
}

void HttpProxy::setWriteCallback(SocketPtr& sockptr, const EventFunc& func) {
        auto cnptr = loop_.getChannel(sockptr);
        cnptr->setWriteCallback(func); 
}

void HttpProxy::setErrorCallback(SocketPtr& sockptr, const EventFunc& func) {
        auto cnptr = loop_.getChannel(sockptr);
        cnptr->setErrorCallback(func); 
}

void HttpProxy::enableReadEvent(SocketPtr& sockptr) { 
        auto cnptr = loop_->getChannel(clientPtr_);
        if(cnptr) {
            cnptr-> enableReadEvent();
        }
}

void HttpProxy::disableReadEvent(SocketPtr& sockptr { 
    auto cnptr = loop_->getChannel(sockptr);
    if (cnptr) {
        cnptr-> disableReadEvent();
    }
}

void HttpProxy::enableWriteEvent(SocketPtr& sockptr) {
    auto cnptr = loop_->getChannel(sockptr);
    if(cnptr) {
        cnptr-> enableWriteEvent();
    }
}

void HttpProxy::disableWriteEvent(SocketPtr& sockptr) {
    auto cnptr = loop_->getChannel(sockptr);
    if(cnptr) {
        cnptr-> disableWriteEvent();
    }
}

    
};

}
#endif
