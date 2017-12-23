#include <sys/timerfd.h>
#include <set>
#include <unordered_set>
#include <memory>
#include "net/TimeWheelEvent.h"
#include "net/SocketUtil.h"
#include "net/EventsLoop.h"


namespace tigerso {

TimeWheelEvent::TimeWheelEvent(const int nodenum): _nodeNum(nodenum) {
        init();
}

TimeWheelEvent::~TimeWheelEvent() { destory(); }

int TimeWheelEvent::register2EventsLoop(EventsLoop& loop) {
    /*create timerfd*/
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK| TFD_CLOEXEC);
    tfd = SocketUtil::RelocateFileDescriptor(tfd, TIMERFD_FD_BASE);
    _timerfd.setSocket(tfd);

    struct itimerspec timevalue;
    timevalue.it_value.tv_sec =  TIMEWHEEL_INTERVAL_SECOND;
    timevalue.it_value.tv_nsec = 0;
    timevalue.it_interval.tv_sec = TIMEWHEEL_INTERVAL_SECOND;
    timevalue.it_interval.tv_nsec = 0;
    timerfd_settime(tfd, 0, &timevalue, NULL);

    loop.registerChannel(_timerfd);
    if(nullptr == _timerfd.channelptr) {
        DBG_LOG("Failed to reigster to EventsLoop, errno:%d, reason: %s", errno, strerror(errno));
        return -1;
    }

    _timerfd.channelptr->setReadCallback(std::bind(&TimeWheelEvent::clockEventHandle, this, std::placeholders::_1));
    _timerfd.channelptr->enableReadEvent();
    return 0;
}

int TimeWheelEvent::clockEventHandle(TimerFd& tfd) {
    //DBG_LOG("Time wheel clock alert handle.");
    char buffer[TIMERFD_READ_SIZE] = {0};
    int ret = ::read(_timerfd, buffer, sizeof(buffer));
    if(ret != TIMERFD_READ_SIZE) { 
        DBG_LOG("Error, not clock alert event, continue wait");
        return -1;
    }
    clockJump();
    return 0;
}

int TimeWheelEvent::clockErrorHandle(TimerFd& tfd) {
    DBG_LOG("Timer Wheel Error hander.");
    _timerfd.close();
    return 0;
}

int TimeWheelEvent::updateChannel(Channel* cnptr) {
    if(isSkipChannel(cnptr)) { return 0; }
    /*erase this channel from node list, and inset it into previous node*/
    DBG_LOG("update timeout channel: %ld", cnptr);
    eraseChannel(cnptr);
    _current->ChannelSet.insert(cnptr);
    return 0;
}

int TimeWheelEvent::eraseChannel(Channel* cnptr) {
    //DBG_LOG("erase channel ptr: %ld", cnptr);
    TimeNode* cur = _current;
    do {
        if(!deleteChannelFromTimeNode(cur, cnptr)) { return 0; }
        cur = cur->next;
    }
    while(cur != _current);
    return 0;
}

void TimeWheelEvent::clockJump() {
    _current = _current->next;
    clearCurrentTimeNode();
}

/*when clock jump event happen, close timeout sockfd in this node, and clear it*/
int TimeWheelEvent::clearCurrentTimeNode() {
    /*
    printlist();
    for(auto item: _current->ChannelSet) {
        DBG_LOG("timeout list channelptr: %ld", item);
    }
    */

    //DBG_LOG("Timeout Node id: %d, size: %ld", _current->id, _current->ChannelSet.size());
    while(_current->ChannelSet.size()) {
       auto it = _current->ChannelSet.begin();
       //DBG_LOG("Channel ptr: %ld, current timeout list size: %lu", *it,  _current->ChannelSet.size());

       Channel* cnptr = *it;
       if(nullptr != cnptr && cnptr->sockfd != -1 && nullptr != cnptr->getSocketPtr()){ 
            Socket* sockptr = cnptr->getSocketPtr();
            DBG_LOG("Time Wheel close socket [%d]", sockptr->getSocket());    
            sockptr->close();
       }
        _current->ChannelSet.erase(cnptr);
    }
    _current->ChannelSet.clear();
    return 0;
}

bool TimeWheelEvent::containsInCurrentTimeNode(Channel* cnptr) {
    if(nullptr == cnptr) {
        return false;
    }
    if(_current->ChannelSet.find(cnptr) != _current->ChannelSet.end()) {
        return true;
    }
    return false;
}

int TimeWheelEvent::deleteChannelFromTimeNode(TimeNode* node, Channel* cnptr) {
    if(nullptr == node || nullptr == cnptr) { return -1; }

    //DBG_LOG("node id: %d, size: %lu", node->id, node->ChannelSet.size());
    /*Find channel in this node, erase it*/
    if(node->ChannelSet.find(cnptr) != node->ChannelSet.end()) {
        node->ChannelSet.erase(cnptr);
        return 0;
    }
    return -1;
}

void TimeWheelEvent::init() {
    /*create circle list*/
    _head = new TimeNode;
    _head->id = _nodeNum;
    int num = _nodeNum;
    TimeNode* cur = _head;
    while(num-- > 1) {
       cur->next = new TimeNode;
       cur = cur->next;
       cur->id = num;
    }
    cur->next = _head;
    _current = _head;
}

void TimeWheelEvent::destory() {
    TimeNode* cur = _head->next;
    TimeNode* ne = cur->next;
    while(cur != nullptr && cur != _head) { 
        cur->next = nullptr;
        delete cur;
        cur = ne;
        ne = cur->next;
    }
    delete _head;
}

bool TimeWheelEvent::isSkipChannel(Channel* cnptr) {
    if(nullptr == cnptr) { return true; }
    if(cnptr->sockfd == -1) { return true; }

    Socket* sockptr = cnptr->getSocketPtr();
    if(sockptr != nullptr) {
        //skip
        if(sockptr->isListening() || sockptr == &_timerfd) {
            return true;
        }
        return false;
    }
    return true;
}

void TimeWheelEvent::printlist() {
        TimeNode* cur = _current;
        int num = 0;
        do{
            if(cur == NULL) {
                DBG_LOG(">>>>>FATAL, cur is NULL");
            }
            num ++;
            DBG_LOG(">>>>>>Node ID: %d", cur->id);
            cur = cur->next;
        }
        while(cur != _current);
        DBG_LOG(">>>> list num is %d", num);
        if(cur == nullptr) {
            DBG_LOG(">>>>>FATAL, cur is NULL");
        }

    }

}
