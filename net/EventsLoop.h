#ifndef TS_NET_EVENTSLOOP_H_
#define TS_NET_EVENTSLOOP_H_

#include <iostream>
#include <sys/epoll.h>
#include <errno.h>
#include <string.h>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <string.h>
#include "core/Logging.h"
#include "core/BaseClass.h"
#include "net/Socket.h"
#include "net/Channel.h"
#include "net/TimeWheelEvent.h"

namespace tigerso {

//typedef std::shared_ptr<Channel> ChannelPtr;

static const int MAX_CHANNEL_NUM = 512;
static const int DEFAULT_CHANNEL_NUM = 128;

class EventsLoop: public nocopyable {

public:
    EventsLoop(const int channels = DEFAULT_CHANNEL_NUM);

    int registerChannel(Socket&);
    int unregisterChannel(Socket&);
    void setTimeout(const int time);
    int getEpollBase() const;
    int updateChannel(Channel*);
    int loop();
    int stop(); 

    ~EventsLoop();

private:
    int waitChannel();
    int createEpollBase();
    int ctlChannel(Channel*, const int op);
    int addChannel(Channel*);
    int removeChannel(Channel*);
    evf_t transFlag(Channel*);
    int cleanNeedDeletedChannels();

private:
    epoll_event epevents_[MAX_CHANNEL_NUM];
    std::set<Channel*> needDeletedChannelSet_;
    const int channelNum_ = DEFAULT_CHANNEL_NUM;
    int waitTime_ = 10000; //10s
    bool loop_ = false;
    int epfd_ = -1;
    TimeWheelEvent timer_;

};

}//namespace mcutil
#endif
