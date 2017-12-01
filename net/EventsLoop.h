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

namespace tigerso::net {

//typedef std::shared_ptr<Channel> ChannelPtr;

static const int MAX_CHANNEL_NUM = 1024;
static const int DEFAULT_CHANNEL_NUM = 512;

class EventsLoop: public core::nocopyable {
    struct EventData {
        Channel* cnptr = nullptr;
        int sockfd = -1;
        int alive = 0;
    };
    typedef EventData* DataPtr;
public:
    EventsLoop(const int channels = DEFAULT_CHANNEL_NUM) 
        :channelNum_(channels) {
        createEpollBase();
        std::cout << "Epoll createdm socket: " << epfd_ << std::endl;
    }

    int registerChannel(Socket&);
    int unregisterChannel(Socket&);

    void setTimeout(const int time);
    int getEpollBase() const;
    int updateChannel(Channel*);
    int loop();
    int stop() {loop_ = false;}

    ~EventsLoop() {
        std::cout << "close epoll fd [" << epfd_ << "]" << std::endl; 
        ::close(epfd_);
        epfd_ = -1;
    }

private:
    int waitChannel();
    int createEpollBase();
    int ctlChannel(Channel*, const int op);
    int addChannel(Channel*);
    int removeChannel(Channel*);
    evf_t transFlag(Channel*);
    int cleanNeedDeletedChannels();

private:
    int epfd_ = -1;
    epoll_event epevents_[MAX_CHANNEL_NUM];
    const int channelNum_ = DEFAULT_CHANNEL_NUM;
    int waitTime_ = 10000; //10s
    bool loop_ = false;

private:
    std::set<Channel*> needDeletedChannelSet_;

};

}//namespace mcutil
#endif
