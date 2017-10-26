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

class TcpConnection {

private:
    string peerIPstr_;
    string port_;
};

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
int tunnelReadServerCallback(SocketPtr& serverptr);
int tunnelWriteServerCallback(SocketPtr& serverptr);
int tunnelReadClientCallback(SocketPtr& clientptr);
int tunnelWriteClientCallback(SocketPtr& clientptr);

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

int tunnelReadServerCallback(SocketPtr& serverptr) {
    cout << "Tunnel: server [" << serverptr->getSocket() << "] read event" <<endl;
    int res = serverptr->recvNIO();
        cout << "recv "<< res << " bytes, total " << serverptr->getInBufferPtr()->getReadableBytes() << " bytes in In Buffer" <<endl;
    if(res == 0) {
        cout << "Tunnel: server [" << serverptr->getSocket() <<"] read 0 bytes, continue wait read event";
        return EVENT_CALLBACK_CONTINUE;
    }
    else if(res > 0) {
        auto clientptr = getClientSocketPtr(serverptr);
        auto cnptr = clientptr->getChannel();
        cnptr->setWriteCallback(tunnelWriteClientCallback);
        cnptr->enableWriteEvent();
        return EVENT_CALLBACK_CONTINUE;
    }

    serverptr->close();
    return EVENT_CALLBACK_BREAK;
}

int tunnelWriteServerCallback(SocketPtr& serverptr) {
    cout << "Tunnel: client [" << serverptr->getSocket() << "] write event" <<endl;
    int res = serverptr->sendNIO();
    auto bufptr = serverptr->getOutBufferPtr();
    auto cnptr = serverptr->getChannel();
    cout << "send "<< res << " bytes, left " << bufptr->getReadableBytes() << " bytes in Out Buffer" <<endl;
    if(bufptr->getReadableBytes() == 0) {
        cnptr->disableWriteEvent();    
        return EVENT_CALLBACK_BREAK;
    }

    if(res >= 0) {
        auto cnptr = serverptr->getChannel();
        //keep send the left data
        if(bufptr->getReadableBytes() > 0) {
            cnptr->setWriteCallback(tunnelWriteServerCallback);
            cnptr->enableWriteEvent();    
            return EVENT_CALLBACK_CONTINUE;
        }

        cnptr->setReadCallback(tunnelReadServerCallback);
        cnptr->enableReadEvent();
        cnptr->disableWriteEvent();
        return EVENT_CALLBACK_CONTINUE;
    }
    else {
        serverptr->close();
        return EVENT_CALLBACK_BREAK;
    }
    return EVENT_CALLBACK_BREAK;
}

int tunnelReadClientCallback(SocketPtr& clientptr) {
    cout << "Tunnel: client [" << clientptr->getSocket() << "] write event" <<endl;
    int res = clientptr->recvNIO();
    auto serverptr = getServerSocketPtr(clientptr);
    auto cnptr = serverptr->getChannel();
    cout << "recv "<< res << " bytes, total " << clientptr->getInBufferPtr()->getReadableBytes() << " bytes in In Buffer" <<endl;
    if(res == 0) {
        cout << "Tunnel: client [" << clientptr->getSocket() <<"] read 0 bytes, continue wait read event";
        clientptr->getChannel()->enableReadEvent();
        return EVENT_CALLBACK_CONTINUE;
    }
    else if(res > 0) {
        cnptr->setWriteCallback(tunnelWriteServerCallback);
        cnptr->enableWriteEvent();
        return EVENT_CALLBACK_CONTINUE;
    }

    clientptr->close();
    deleteProxyConnections(clientptr);
    return EVENT_CALLBACK_BREAK;
}

int tunnelWriteClientCallback(SocketPtr& clientptr) {
    cout << "Tunnel: client [" << clientptr->getSocket() << "] write event" <<endl;
    int res = clientptr->sendNIO();
    auto cnptr = clientptr->getChannel();
    auto bufptr = clientptr->getOutBufferPtr();
    cout << "send "<< res << " bytes, left " << bufptr->getReadableBytes() << " bytes in Out Buffer" <<endl;
    if(bufptr->getReadableBytes() == 0) {
        cnptr->disableWriteEvent();
        return EVENT_CALLBACK_BREAK;
    }

    if(res >= 0) {

        //keep send the left data
        if(bufptr->getReadableBytes() > 0) {
            cnptr->setWriteCallback(tunnelWriteClientCallback);
            cnptr->enableWriteEvent();    
            return EVENT_CALLBACK_CONTINUE;
        }

        cnptr->setReadCallback(tunnelReadClientCallback);
        cnptr->enableReadEvent();
        cnptr->disableWriteEvent();
        return EVENT_CALLBACK_CONTINUE;
    }
    else {
        clientptr->close();
        deleteProxyConnections(clientptr);
        return EVENT_CALLBACK_BREAK;
    }
    return EVENT_CALLBACK_BREAK;
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
            string::size_type  pos = domain.find(":");
            if(pos != string::npos) {
                domain = domain.substr(0, pos);
            }
            vector<string> hosts;
            SocketUtil::ResolveHost2IP(domain, hosts);
            cout << " >> + Domain: " << domain << endl;

            for(auto &item : hosts) {
                cout << " >> - Host: " << item << endl;
            }

            request.removeHeader("proxy-connection");
            request.markTrade();

            string port = request.getHostPort();
            cout << "Port: " << port <<endl;
            auto inptr = sockptr->getInBufferPtr();
            inptr->clear();
            inptr->addData(request.toString());
            if(hosts.empty()) {
                cout << "host is empty" <<endl;
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
                SocketUtil::Connect(*server, hosts[0], port.c_str());
                if(server->exist()) {
                    server->setNIO(true);
                    cout<< "socket = " << server->getSocket() << endl;
                }

                linkServerSocketPtr(sockptr, server);
                ChannelPtr cnptr = eloop.getChannel(server);
                cout << "Rebind sockets buffer" <<endl;
                server->setOutBufferPtr(sockptr->getInBufferPtr());
                sockptr->setOutBufferPtr(server->getInBufferPtr());

                //Tunnel SSL traffic
                if (strcasecmp(request.getMethod().c_str(),"CONNECT") == 0) {
                    sockptr->getInBufferPtr()->clear();
                     
                    cout << "Enter SSL traffic tunnel flows" <<endl;
                    HttpResponse reps;
                    reps.setVersion("HTTP/1.1");
                    reps.setStatuscode(200);
                    reps.setDesc("Connection established");
                    reps.appendHeader("Proxy-agent", "tigerso/1.0.0");
                    cout << reps.toString() <<endl;
                    sockptr->getOutBufferPtr()->addData(reps.toString());
                    cnptr->enableReadEvent();
                    cnptr->setReadCallback(tunnelReadServerCallback);

                    sockptr->getChannel()->setWriteCallback(tunnelWriteClientCallback);
                    sockptr->getChannel()->enableWriteEvent();
                    sockptr->getChannel()->setReadCallback(tunnelReadClientCallback);
                    sockptr->getChannel()->enableReadEvent();
                }
                else {
                //socklist.push_back(server);
                    cnptr->setWriteCallback(writeCallback);
                    cnptr->enableWriteEvent();
                }
            }
        }
        else if (sockptr->getRole() == SOCKET_ROLE_SERVER) {
            HttpResponse response;
            parser.parse(instr, response);

            cout <<"\tResponse info:\n " << "Code: " << response.getStatuscode() << "\n\tDesc: " << response.getDesc() << endl;
            string content_length = response.getValueByHeader("content-length");
            cout<< "\tcontent-length: " << content_length <<endl;
            cout << "\tbody length: " << response.getBody().size() <<endl;
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
        auto bufptr = sockptr->getOutBufferPtr();

        //keep send the left data
        if(bufptr->getReadableBytes() > 0) {
            cnptr->setWriteCallback(writeCallback);
            cnptr->enableWriteEvent();    
            return EVENT_CALLBACK_CONTINUE;
        }

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


