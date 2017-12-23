#ifndef TS_NET_TIMEWHEELEVENT_H_
#define TS_NET_TIMEWHEELEVENT_H_

#include <sys/timerfd.h>
#include <set>
#include <unordered_set>
#include <memory>
#include "core/BaseClass.h"
#include "net/Socket.h"
#include "net/SocketUtil.h"

namespace tigerso {

class EventsLoop;
class TimeWheelEvent: public nocopyable {

#define TIMERFD_READ_SIZE 8
#define TIMERFD_FD_BASE 123
#define TIMEWHEEL_INTERVAL_SECOND 2
#define TimerFd Socket

typedef struct _TimeNode_st{
    _TimeNode_st(){ id ++; }
    std::unordered_set<Channel*> ChannelSet;
    _TimeNode_st* next = nullptr;
    int id = 0;
} TimeNode;

public:
    TimeWheelEvent(const int nodenum = 5);
    ~TimeWheelEvent();

    int register2EventsLoop(EventsLoop& loop);
    int clockEventHandle(TimerFd& tfd);
    int clockErrorHandle(TimerFd& tfd);
    int updateChannel(Channel* cnptr);
    int eraseChannel(Channel* cnptr);
    void printlist();

private:
    void clockJump();
    int clearCurrentTimeNode();
    bool containsInCurrentTimeNode(Channel* cnptr);
    int deleteChannelFromTimeNode(TimeNode* node, Channel* cnptr);    
    bool isSkipChannel(Channel*);
    void init();
    void destory();

private:
    TimeNode* _current= nullptr;
    TimeNode* _head = nullptr;
    const int _nodeNum;
    TimerFd _timerfd;
};

}
#endif
