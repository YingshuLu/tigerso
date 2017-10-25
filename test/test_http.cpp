#include <iostream>
#include <typeinfo>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>
#include "stdio.h"
#include "net/Socket.h"
#include "net/SocketUtil.h"
#include "net/EventsLoop.h"
#include "net/Channel.h"
#include "http/HttpMessage.h"
#include "util/FileTypeDetector.h"

using namespace std;
using namespace tigerso::net;
using namespace tigerso::http;

class ProxyConnections {

public:

    explicit ProxyConnections(SocketPtr& clientptr, SocketPtr& serverptr) {
        clientptr_ = clientptr;
        serverptr_ = serverptr;
    }

    explicit ProxyConnections(SocketPtr& sockptr) {
        if(sockptr->exist() && sockptr->getRole() == SOCKET_ROLE_CLIENT) {
            clientptr_ = sockptr;
        }
    }

    bool operator==(const ProxyConnections& cnns) {
        if(clientptr_ == nullptr || cnns.clientptr_ == nullptr) {
            return false;
        }
        return clientptr_->getSocket() == cnns.clientptr_->getSocket();
    }

    bool linkServer(SocketPtr& sockptr) {
        if(serverptr_ != nullptr) {
            serverptr_->close();
            serverptr_ = nullptr;
        }

        if(sockptr->exist() && sockptr->getRole() == SOCKET_ROLE_SERVER) {
            serverptr_ = sockptr;
            return true;
        }
        return false;
    }

    bool unlinkServer(SocketPtr& sockptr) {
        if(serverptr_ != nullptr) {
            serverptr_->close();
            serverptr_ = nullptr;
        }
        return true;
    }

    void closeProxy() {
        if(serverptr_ != nullptr) {serverptr_->close();}
        if(clientptr_ != nullptr) {clientptr_->close();}
    }
    /*~ProxyConnections() {
        closeProxy();
    }*/

    SocketPtr clientptr_ = nullptr;
    SocketPtr serverptr_ = nullptr;

};

EventsLoop eloop;
vector<SocketPtr> socklist;
HttpParser parser;

vector<ProxyConnections> httpproxypool;
SocketPtr getClientSocketPtr(SocketPtr& serverptr) {
    for (auto it = httpproxypool.begin(); it != httpproxypool.end(); it++) {
        if(serverptr->getSocket() == it->serverptr_->getSocket()) {
            return it->clientptr_;
        }
    }
    return nullptr;
}

SocketPtr getServerSocketPtr(SocketPtr& clientptr) {
    for (auto it = httpproxypool.begin(); it != httpproxypool.end(); it++) {
        if(clientptr->getSocket() == it->clientptr_->getSocket()) {
            return it->serverptr_;
        }
    }
    return nullptr;
}

bool linkServerSocketPtr(SocketPtr& clientptr, SocketPtr& serverptr) {
    for (auto it = httpproxypool.begin(); it != httpproxypool.end(); it++) {
        if(clientptr->getSocket() == it->clientptr_->getSocket()) {
            it->linkServer(serverptr);
            return true;
        }
    }

    ProxyConnections cnns = ProxyConnections(clientptr, serverptr);
    httpproxypool.push_back(cnns);
    return true;

}

bool deleteProxyConnections(SocketPtr& clientptr) {
    for (auto it = httpproxypool.begin(); it != httpproxypool.end(); it++) {
        if(clientptr->getSocket() == it->clientptr_->getSocket()) {
            httpproxypool.erase(it);
            return true;
        }
    }
    return false;

}

int deleteFromSocketList(const SocketPtr& sockptr) {
    auto iter = std::find(socklist.begin(), socklist.end(), sockptr);
    if(iter != socklist.end()) {
        socklist.erase(iter);
    }
    return 0;
}

int beforeCallback(SocketPtr&);
int afterCallback(SocketPtr&);
int errorCallback(SocketPtr&);
int readCallback(SocketPtr&);
int writeCallback(SocketPtr&);
int acceptMasterSocket(SocketPtr&);

int tcpServer(SocketPtr& master) {
    int ret = SocketUtil::CreateListenSocket("","8080",true, *master);
    cout<< "return code: "<< ret << "\nlistening socket [" << master->getSocket() << "]\nIP: " << master->getStrAddr() << "\nPort: " << master->getStrPort() <<endl;
    
    ChannelPtr cnptr = eloop.getChannel(master);
    master->setNIO(true);
    if(cnptr == nullptr) {
        cout << "cnptr is nullptr" << endl;
    }
    cnptr->setBeforeCallback(beforeCallback);
    cnptr->setAfterCallback(afterCallback);
    cnptr->setErrorCallback(errorCallback);
    cnptr->setReadCallback(acceptMasterSocket);
    cnptr->enableReadEvent();

    return 0;
}

int main() {
    SocketPtr master(new Socket());
    tcpServer(master);
    eloop.loop();
    return 0;
}

int beforeCallback(SocketPtr& sockptr) {
    cout << "- before Callback, socket ["<< sockptr->getSocket() <<"]"<<endl;
    return EVENT_CALLBACK_CONTINUE;
}

int afterCallback(SocketPtr& sockptr) {
    cout << "- After Callback, socket ["<< sockptr->getSocket() <<"]"<<endl;
    return EVENT_CALLBACK_CONTINUE;
}

int errorCallback(SocketPtr& sockptr) {
    cout << "- Error Callback, socket ["<< sockptr->getSocket() <<"]"<<endl;
    sockptr->close();
    deleteFromSocketList(sockptr);
    return EVENT_CALLBACK_BREAK;
}

int readCallback(SocketPtr& sockptr) {
    
    cout << "- Read Callback, socket ["<< sockptr->getSocket() <<"]"<<endl;
    if(sockptr->recvNIO() >= 0) {
        auto bufptr = sockptr->getInBufferPtr();
        string instr = bufptr->toString();
            
        // parse Http Message
        if(sockptr->getRole() == SOCKET_ROLE_CLIENT) {
            HttpRequest request;
            parser.parse(instr, request);

            cout<< " >> Request info:\n" << "    Method: " << request.getMethod() << "\n    Host:" << request.getValueByHeader("host") <<endl;
            string outstr = HttpResponse::NOT_FOUND;
            string domain = request.getValueByHeader("host");
            vector<string> hosts;
            SocketUtil::ResolveHost2IP(domain, hosts);
            cout << " >> + Domain: " << domain << endl;

            for(auto &item : hosts) {
                cout << " >> - Host: " << item << endl;
            }

            auto outptr = sockptr->getOutBufferPtr();
            if(hosts.empty()) {
                sockptr->close();
                return EVENT_CALLBACK_BREAK;
                /*
                outptr->addData(outstr);
                ChannelPtr cnptr = sockptr->getChannel();
                cnptr->setWriteCallback(writeCallback);
                cnptr->enableWriteEvent();
                */
            }
            else {
                SocketPtr server = std::make_shared<Socket>();
                SocketUtil::Connect(*server, hosts[0], "80");
                if(server->exist()) {
                    cout<< "socket = " << server->getSocket() << endl;
                }
                server->setOutBufferPtr(sockptr->getInBufferPtr());
                sockptr->setOutBufferPtr(server->getInBufferPtr());
                linkServerSocketPtr(sockptr, server);
                //socklist.push_back(server);
                ChannelPtr cnptr = eloop.getChannel(server);
                cnptr->setWriteCallback(writeCallback);
                cnptr->enableWriteEvent();
            }
        }
        else if (sockptr->getRole() == SOCKET_ROLE_SERVER) {
            HttpResponse response;
            parser.parse(instr, response);

            cout <<"\tResponse info:\n " << "Code: " << response.getStatuscode() << "\n\tDesc: " << response.getDesc() << endl;
            string content_length = response.getValueByHeader("content-length");
            cout<< "\tcontent-length: " << content_length <<endl;
            int length = 0;
            if(!content_length.empty())
            {
                length = atoi(content_length.c_str());
            }

            SocketPtr clientptr = getClientSocketPtr(sockptr);
            if(clientptr != nullptr) {
                clientptr->setOutBufferPtr(sockptr->getInBufferPtr());
                sockptr->setOutBufferPtr(clientptr->getInBufferPtr());
                cout << "Get client socket: [" << clientptr->getSocket() << "]" <<endl;
                ChannelPtr cnptr = eloop.getChannel(clientptr);
                cout << "Get client channel: "<< cnptr <<", socket: [" << clientptr->getSocket() << "]" <<endl;
                
                cnptr->setWriteCallback(writeCallback);
                cnptr->enableWriteEvent();
                cnptr->enableReadEvent();
            }

        }
         return EVENT_CALLBACK_CONTINUE;
    }
    else {
        sockptr->close();
        deleteFromSocketList(sockptr);
        return EVENT_CALLBACK_BREAK;
    }
    return EVENT_CALLBACK_BREAK;
}

int writeCallback(SocketPtr& sockptr) {

    cout << "- Write Callback, socket ["<< sockptr->getSocket() <<"]"<<endl;
    if(!SocketUtil::TestConnect(*sockptr)) {
        
        cout<< "socket = " << sockptr->getSocket() << "closed" << endl;
        sockptr->close();
        return EVENT_CALLBACK_BREAK;
    }

    if(sockptr->sendNIO() >= 0) {
        auto cnptr = sockptr->getChannel();
        cnptr->setReadCallback(readCallback);
        cnptr->disableWriteEvent();
        cnptr->enableReadEvent();
        return EVENT_CALLBACK_CONTINUE;
    }
    else {
        sockptr->close();
        return EVENT_CALLBACK_BREAK;
    }
    return EVENT_CALLBACK_BREAK;
}

int acceptMasterSocket(SocketPtr& master) {
    
    std::cout << "+ Accept handle" << endl;
    while(master) {
        SocketPtr sockptr = make_shared<Socket>();
        int ret = SocketUtil::Accept(*master, *sockptr);
        if (ret != 0) {
            std::cout << "Accept error" << endl;
            return EVENT_CALLBACK_CONTINUE;
        }
        
        cout << "- Accept socket [" << sockptr->getSocket() << "] from " << sockptr->getStrAddr() << ":" << sockptr->getStrPort() <<endl;
    
        sockptr->setNIO(true);
        ChannelPtr cnptr = eloop.getChannel(sockptr);
        cnptr->setBeforeCallback(beforeCallback);
        cnptr->setAfterCallback(afterCallback);
        cnptr->setErrorCallback(errorCallback);
        cnptr->setReadCallback(readCallback);
        cnptr->enableReadEvent();
        cout << "save socketptr into socketlist" << endl;
        ProxyConnections cnns = ProxyConnections(sockptr);
        httpproxypool.push_back(cnns);
        //socklist.push_back(sockptr);
    }
    
    return EVENT_CALLBACK_CONTINUE;
}


