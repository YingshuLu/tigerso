#ifndef TS_NET_EVENTSLOOP_H_
#define TS_NET_EVENTSLOOP_H_

#include <iostream>
#include <sys/epoll.h>
#include <errno.h>
#include <string.h>
#include <vector>
#include <map>
#include <memory>
#include <string.h>
#include "core/Logging.h"
#include "core/BaseClass.h"
#include "net/Socket.h"
#include "net/Channel.h"

namespace tigerso::net {

typedef std::shared_ptr<Channel> ChannelPtr;

static const int MAX_CHANNEL_NUM = 10240;
static const int DEFAULT_CHANNEL_NUM = 1024;

class EventsLoop: public core::nocopyable {
public:
    EventsLoop(const int channels = DEFAULT_CHANNEL_NUM) 
        :channelNum_(channels) {
        createEpollBase();
    }

    ChannelPtr getChannel(const SocketPtr&);
    int registerChannel(const ChannelPtr&);
    int unregisterChannel(const ChannelPtr&);
    bool contains(const ChannelPtr&);
    bool contains(const SocketPtr&);

    void setTimeout(const int time);
    int getEpollBase() const;
    int updateChannel(const ChannelPtr&);
    int loop();

    void displayChannelList() const;

private:
    int waitChannel();
    void loopEvents();
    int createEpollBase();
    int ctlChannel(const ChannelPtr&, const int op);
    int addChannel(const ChannelPtr&);
    int removeChannel(const ChannelPtr&);
    evf_t transFlag(const ChannelPtr&);

    int epfd_ = -1;
    epoll_event epevents_[MAX_CHANNEL_NUM];
    const int channelNum_ = DEFAULT_CHANNEL_NUM;
    int waitTime_ = 10000;
    bool loop_ = false;
    std::map<socket_t, ChannelPtr> channels_;
};

}//namespace mcutil
#endif
