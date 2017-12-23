#include <iostream>
#include "core/tigerso.h"
#include "net/Channel.h"
#include "net/EventsLoop.h"
#include "net/SocketUtil.h"
#include "core/Logging.h"

namespace tigerso {

#define DECIDE_EVENTCALLBACK(ret) {\
        if(ret == EVENT_CALLBACK_BREAK) { timer_.updateChannel(cnptr); continue; }\
        else if (ret == EVENT_CALLBACK_DROPWAITED){ \
            cleanNeedDeletedChannels();\
            return EVENT_CALLBACK_DROPWAITED;\
        }\
} ret == ret

#define validChannel(cnptr) (cnptr && cnptr->sockfd > 0)

int EventsLoop::registerChannel(Socket& socket) {
    if (!socket.exist()) { return -1; }

    if(socket.channelptr == nullptr) {
        //Create
        Channel* cnptr = new Channel(*this, socket);
        socket.channelptr = cnptr;
        cnptr->sockfd = socket.getSocket();
        timer_.updateChannel(cnptr);
        return addChannel(cnptr);
        
    }
    return updateChannel(socket.channelptr);
}

int EventsLoop::unregisterChannel(Socket& socket) {
    //Should unregister before
    if(socket.channelptr == nullptr) { return 0; }
    removeChannel(socket.channelptr);
    socket.channelptr->sockfd = -1;

    needDeletedChannelSet_.insert(socket.channelptr);
    //Delete channel
    //delete socket.channelptr;
    timer_.eraseChannel(socket.channelptr);
    socket.channelptr = nullptr;
    DBG_LOG("unregister Socket [%d]",socket.getSocket());
    return 0;
}

void EventsLoop::setTimeout(const int time) {
    waitTime_ = time;
}

int EventsLoop::getEpollBase() const {
    return epfd_;
}

int EventsLoop::loop() {
    loop_ = true;
    timer_.register2EventsLoop(*this);
    while(loop_) {
        waitChannel();
    }
    return true;
}

int EventsLoop::waitChannel() {
        Channel* cnptr = nullptr;
        Socket* sockptr = nullptr;
        memset(epevents_, 0, sizeof(epevents_));

        int num = epoll_wait(epfd_, epevents_, channelNum_, waitTime_);
        if(num == -1) {
            loop_ = false;
            INFO_LOG("epoll wait return -1, error: %d, %s", errno, strerror(errno));
            return -1;
        }
        //DBG_LOG("Epoll detected %d sockets", num);
        for(int i = 0; i < num; i++) {
            cnptr = nullptr;
            sockptr = nullptr;

            cnptr = static_cast<Channel*>(epevents_[i].data.ptr);
            if(!validChannel(cnptr)) { 
                DBG_LOG("This Socket has been destoryed, now skip its events");
                continue;
            }

            sockptr = cnptr->getSocketPtr();
            int fd = sockptr->getSocket();
         
            assert(sockptr);
            if(!sockptr->exist()) {
                DBG_LOG("socket is closed in registered list");
                unregisterChannel(*sockptr);
                continue;
            }

           // cnptr = sockptr->channelptr;
           //DBG_LOG("Detected socket [%d] Events", fd);

            if(cnptr->before_cb && validChannel(cnptr)) {
                DBG_LOG("socket [%d]: BEFORE event", fd);
                int ret = cnptr->before_cb(*sockptr);
                DECIDE_EVENTCALLBACK(ret);
            }

            if((epevents_[i].events & EPOLLRDHUP) && (epevents_[i].events & EPOLLIN) && validChannel(cnptr)) {
                DBG_LOG("socket [%d]: RDHUP event", fd);
                if (cnptr->rdhup_cb) {
                    int ret = cnptr->rdhup_cb(*sockptr);
                    DECIDE_EVENTCALLBACK(ret);
                }
            }

            if((epevents_[i].events & EPOLLHUP) && validChannel(cnptr)) {
                DBG_LOG("socket [%d]: HUP event", fd);
                if (cnptr->error_cb) {
                    int ret = cnptr->error_cb(*sockptr);
                    DECIDE_EVENTCALLBACK(ret);
                }
            }

            if((epevents_[i].events & EPOLLERR) && validChannel(cnptr)) {
                DBG_LOG("socket [%d]: ERROR event", fd);
                if (cnptr->error_cb) {
                    int ret = cnptr->error_cb(*sockptr);
                    DECIDE_EVENTCALLBACK(ret);
                }
            }

            if((epevents_[i].events & EPOLLIN) && validChannel(cnptr)) {
                DBG_LOG("socket [%d]: READ event", fd);
                if(cnptr->readable_cb) {
                    int ret = cnptr->readable_cb(*sockptr);
                    DECIDE_EVENTCALLBACK(ret);
                }
            }

            if((epevents_[i].events & EPOLLOUT) && validChannel(cnptr)) {
                DBG_LOG("socket [%d]: WRITE event", fd);
                if(cnptr->writeable_cb) {
                    int ret = cnptr->writeable_cb(*sockptr);
                    DECIDE_EVENTCALLBACK(ret);
                 }
            }

            if(cnptr->after_cb && validChannel(cnptr)) {
                DBG_LOG("socket [%d]: AFTER event", fd);
                int ret = cnptr->after_cb(*sockptr);
                DECIDE_EVENTCALLBACK(ret);
            }
            timer_.updateChannel(cnptr);
        }

        cleanNeedDeletedChannels();
        return num;
}

int EventsLoop::createEpollBase() {
    if(epfd_ == -1) {
        epfd_ = 128;
        int tempfd = epoll_create(channelNum_);
        if(tempfd < 0) {
            loop_ = false;
            return -1;
        }
        epfd_ = SocketUtil::RelocateFileDescriptor(tempfd, 256);
    }
    return 0;
}

int EventsLoop::updateChannel(Channel* cnptr) {
    if(ctlChannel(cnptr, EPOLL_CTL_MOD) == -1) {
        if(errno == ENOENT) {
            return ctlChannel(cnptr, EPOLL_CTL_ADD);
        }
        else {
            INFO_LOG( "Mod channel event errno: %d, %s", errno, strerror(errno));
            return -1;
        }
    }
    return 0;
}

int EventsLoop::ctlChannel(Channel* cnptr, const int op) {
    evf_t evts = transFlag(cnptr);
    epoll_event epev;
    //epev.data.fd = cnptr->sockfd;
    epev.events = evts;
    epev.data.ptr = static_cast<Channel*>(cnptr);
    return epoll_ctl(epfd_, op, cnptr->sockfd, &epev); 
}

int EventsLoop::addChannel(Channel* cnptr) {
    auto sp = cnptr->getSocketPtr();
    if(!sp) { return -1; }

    int fd = sp->getSocket();

    DBG_LOG("Add socket [%d] event", fd);
    if(ctlChannel(cnptr, EPOLL_CTL_ADD) == -1) {
        if(errno == EEXIST) {
            INFO_LOG("the supplied [%d] is already in epfd [%d]", fd, epfd_);
            return ctlChannel(cnptr, EPOLL_CTL_MOD);
        }
        else {
            DBG_LOG("epfd[%d] / fd [%d]: internal fatal error:%s", epfd_, fd, strerror(errno));
            return -1;
        }
    }
    return 0;
}

int EventsLoop::removeChannel(Channel* cnptr) {
    socket_t fd = cnptr->sockfd;
    if(ctlChannel(cnptr, EPOLL_CTL_DEL) == -1) {
        if(errno == ENOENT) {
            INFO_LOG("the supplied [%d] is not in epfd [%d]", fd, epfd_);
            return 0;
        }
        else {
            DBG_LOG("epfd[%d] / fd [%d]: internal fatal error:%s", epfd_, fd, strerror(errno));
            return -1;
        }
    }
    return 0;
}

evf_t EventsLoop::transFlag(Channel* cnptr) {
    evf_t evts = 0;
    socket_t sockfd = cnptr->sockfd;
    if(cnptr->events.readFlag) {
        evts |= EPOLLIN;
        //std::cout<<">> enable ["<< sockfd <<"]  read event" << std::endl;
    }
    if(cnptr->events.writeFlag) {
        evts |= EPOLLOUT;
        //std::cout<<">> enable ["<< sockfd <<"] write event" << std::endl;
    }
    if(cnptr->events.hupFlag) {
        evts |= EPOLLRDHUP;
    }
    if(cnptr->events.edgeFlag) {
        evts |= EPOLLET;
    }
    if(!cnptr->events.keepFlag) {
        evts |= EPOLLONESHOT;
    }
    return evts;
}

int EventsLoop::cleanNeedDeletedChannels() {
    auto iter = needDeletedChannelSet_.begin();
    while(iter != needDeletedChannelSet_.end()) {
        delete *iter;
        iter = needDeletedChannelSet_.erase(iter);
    }
    return 0;
}

}
