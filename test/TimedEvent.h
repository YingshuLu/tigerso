#include "net/Channel.h"
#include "net/Socket.h"
#include <set>

using namespace tigerso::net;

struct TimeNode{
    std::set<Channel*> ChannelSet;
    TimeNodeSet* next = nullptr;
};

class TimedEvent {

public:
    TimeEvent(const int nodenum = 4): _nodeNum(nodenum) {
        init();
    }

    ~TimeEvent() { destory(); }

    void clockJump() {
        _current = _current->next;
        clearCurrentTimeNode();
    }

    int updateChannel(Channel* cnptr) {
        if(containsInCurrentTimeNode(cnptr)) {
            return 0;
        }

        TimeNode* ne = _current->next;
        while(ne->next != _current) {
            deleteChannelFromTimeNode(ne, cnptr);
            ++ne;
        }
        _current.ChannelSet.insert(cnptr);
        return 0;
    }

    int deleteChannel(Channel* cnptr) {
        TimeNode* cur = _current;
        while(cur->next != _current) {
            if(!deleteChannelFromTimeNode(cur, cnptr)) { return 0; }
            cur = cur->next;
        }
        return 0;
    }

private:

    /*when clock jump event happen, close timeout sockfd in this node, and clear it*/
    int clearCurrentTimeNode() {
        for (auto it = _current->ChannelSet.begin(); it != _current->ChannelSet.end(); ++it) {
           if(*it->sockfd == -1) { continue; }
           Socket* sockptr = cnptr->getSocketPtr();
           sockptr->close();
        }
        _current->ChannelSet.clear();
        return 0;
    }

    bool containsInCurrentTimeNode(Channel* cnptr) {
        if(nullptr == cnptr) {
            return false;
        }
        if(_current->ChannelSet.find(cnptr) != _current->ChannelSet.end()) {
            return true;
        }
        return false;
    }

    int deleteChannelFromTimeNode(TimeNode* node, Channel* cnptr) {
        if(nullptr == node || nullptr == cnptr) { return -1; }
        /*Find channel in this node, erase it*/
        if(node->ChannelSet.find(cnptr) != node->ChannelSet.end()) {
            node->ChannelSet.erase(cnptr);
            return 0;
        }
        return -1;
    }
    
    void init() {
        _head = new TimeNode;
        int num = _nodeNum;
        TimeNode* cur = _head;
        while(num-- > 1) {
           cur->next = new TimeNode;
           cur = cur->next;
        }
        cur->next = _head;
        _current = _head;
    }

    void destory() {
        TimeNode* cur = _head->next
        TimeNode* ne = cur->next;
        while(cur != _head) { 
            cur->next = nullptr;
            delete cur;
            cur = ne;
            ne = cur->next;
        }
        delete _head;
    }


private:
    TimeNode* _current= nullptr;
    TimeNode* _head = nullptr;
    int _nodeNum = 2;
    int _timerfd = -1;
};
