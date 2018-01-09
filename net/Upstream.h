#ifndef _TS_HTTP_UPSTREAM_H_
#define _TS_HTTP_UPSTREAM_H_

#include <string.h>
#include <stdlib.h>
#include "core/tigerso.h"
#include "core/BaseClass.h"
#include "net/SocketUtil.h"
#include "core/Logging.h"

namespace tigerso {

typedef enum _UPSTREAM_WORK_MODE_ {
    UPSTREAM_WORK_SINGLE,
    UPSTREAM_WORK_ROUBDROBIN,
    UPSTREAM_WORK_ADAPTIVE,
    UPSTREAM_WORK_UNKNOWN
} UPSTREAM_WORK_MODE;

class Upstream: public nocopyable {

#define UPSTREAM_MAX_NUMBER 10
struct UpstreamNode{
    bool active = false;
    char IPv4Addr[IPV4_ADDRSIZE] = {0};
    unsigned int port = 0;  
    UpstreamNode* next = nullptr;
    time_t offsets[10] = {-1};
};

public:
    Upstream() {}

    /*return node id*/
    int fetch(const char* ip, unsigned int& port) {
        int ret = 0;
        switch(_mode) {
            UPSTREAM_WORK_SINGLE:
            UPSTREAM_WORK_ROUBDROBIN:
                ret = roundRobin(ip, port);
                break;
            UPSTREAM_WORK_ADAPTIVE:
                ret = -1; 
                break;
            default:
                break;
        }
        return ret;
    }

    bool inactve(unsigned int loc) {
        return activeNode(loc, false);
    }

    bool active(unsigned int loc) {
        return activeNode(loc, true);
    }

    ~Upstream() { destory(); }
    

public:
    int empty() { return nullptr == _root; }
    int size() {
        int num = 0;
        UpstreamNode* cur = _root;
        while(cur != nullptr) {
            num++;
            cur = cur -> next;
            if(cur == _root_) { break; }
        }
        return num;
    }

    UpstreamNode* insertNode(const char* ip, unsigned int port)  {
        if(!socket::ValidateAddr(ip) || size() > UPSTREAM_MAX_NUMBER) { return nullptr; }
        UpstreamNode* tmp = contains(ip, port);
        if(tmp) { return tmp; }

        tmp = new UpstreamNode;
        memcpy(tmp->IPv4Addr, ip, strlen(ip));
        tmp->port = port;
        tmp->active = true;
        
        UpstreamNode* end = _root;
        while(nullptr != cur && end->next != root) { end = end->next; }
        /*node list is empty*/
        if(nullptr == end) {
            _root = tmp;
            _root->next = _root;
            _current = _root;
            return tmp;
        }

        end->next = tmp;
        tmp->next = _root;
        return tmp;
    }

    UpstreamNode* contains(const char* ip, unsigned int port) {
        if(!SocketUtil::ValidateAddr(ip)) { return nullptr; }
        UpstreamNode* cur = _root;
        while(cur != nullptr && cur -> next != _root) {
            if(port == cur->port && memcmp(ip, cur->IPv4Addr, strlen(ip))) { return cur; }
        }
        return nullptr;
    }

    bool testUpstreamThread() {
        
        return true;
    }

private:
    /*return node relative loaction from root*/
    int roundRobin(const char* ip, unsigned int& port) {
        if(empty()) {
            DBG_LOG("upstream node list is empty");
            return -1;
        }

        UpstreamNode* end = _current;
        int loc = 0;
        do {
            if(_current->active) {
                ip = _current->IPv4Addr;
                port = _current->port;
                DBG_LOG("get upstream: %s:%d", ip, port);
                return loc;
            }
            _current = _current->next;
            loc++;
        }
        while(_current != end);
        DBG_LOG("no active upstream found");
        return -1;
    }

    UpstreamNode* locate(unsigned int loc) {
        int locate = 0;
        UpstreamNode* cur = _root;
        while(cur != nullptr) {
           if(locate == loc) {
                return cur;
           }
           locate++;
           cur = cur->next;
        }
        return nullptr;
    }

    bool activeNode(unsigned int loc, bool on) {
        UpstreamNode* node = locate(loc);
        if(nullptr == node) {
            DBG_LOG("can not found the %d node", (int)loc);
            return false;
        }
        node->active = on? true: false;
        return true;
    }

    void destory() {
        UpstreamNode* cur = _root;
        UpstreamNode* ne = nullptr;
        if(nullptr == cur) { return; }

        do {
            ne = cur->next;
            delete cur;
            cur = ne;
        }
        while(cur != _root);
        return;
    }

private:
    UPSTREAM_WORK_MODE _mode;
    UpstreamNode* _root = nullptr;
    UpstreamNode* _current = nullptr;
    

};

} /*end namespace tigerso*/

#endif /*_TS_HTTP_UPSTREAM_H_*/
