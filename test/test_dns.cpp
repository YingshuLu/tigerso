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
#include "http/HttpParser.h"
#include "dns/DNSResolver.h"
#include "util/FileTypeDetector.h"

using namespace std;
using namespace tigerso::net;
using namespace tigerso::http;
using namespace tigerso::dns;
EventsLoop eloop;

DNSResolver DNSInstance;

Socket udpsock;

static int cnt = 0;

int queryIP(const char* host);

int showIPAddress(const char* ipaddr, time_t rttl) {
    string ip;
    time_t ttl;
    int ret = DNSInstance.getAnswer(ip, ttl);
    ret = DNSInstance.queryDNSCache(string("www.baidu.com").c_str(), ip);
    if (ret != DNS_OK) {
        cout << "Failed to get ip address" <<endl;
        return -1;
    }
    if(cnt < 2) {
        return queryIP("www.baidu.com");
    }
    cout << "IP: " << ip << ", ttl: " << ttl <<endl;
    return 0;
}

int queryIP(const char* host) {
    string ip;
    if(DNSInstance.queryDNSCache(host, ip) == 0) {
        cout << "Get Cache: " << ip <<endl;
        return 0;
    }

    DNSInstance.asyncQueryInit(host, udpsock);
    DNSInstance.setCallback(showIPAddress); 
    DNSInstance.asyncQueryStart(eloop, udpsock);
    return 0;
}

int main(int argc, char* argv[]) {
    if(argc < 2) {
        return -1;
    }

    const char* host = argv[1];
    queryIP(host);
    eloop.loop();
    return 0;
}

