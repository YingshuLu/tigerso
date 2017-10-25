#include <iostream>
#include "core/tigerso.h"
#include "net/Channel.h"
#include "net/EventsLoop.h"

namespace tigerso::net {

ChannelPtr EventsLoop::getChannel(const SocketPtr& sockptr) {
    if (sockptr == nullptr || !sockptr->exist()) {
        return nullptr;
    }

    if(!contains(sockptr)) {
        ChannelPtr cp = std::make_shared<Channel>(*this, sockptr);
        sockptr->setChannel(cp);
        registerChannel(cp);
        return cp;
    }
    else {
        sockptr->getChannel();
    }
    //should never be here
    return nullptr;
}

int EventsLoop::registerChannel(const ChannelPtr& cnnl) {
    if(cnnl == nullptr) {
        return -1;
    }

    if (contains(cnnl)) {
        return 0;
    }
    else {
        SocketPtr sockptr = cnnl->getChannelSocket();
        if (sockptr && sockptr->exist()) {
            int fd = sockptr->getSocket();
            channels_.insert(std::pair<int, ChannelPtr>(fd, cnnl));
            addChannel(cnnl);
        }
    }
    displayChannelList();
    return 0;
}

int EventsLoop::unregisterChannel(const ChannelPtr& cnnl) {

    if(cnnl == nullptr || !contains(cnnl)) {
        return -1;
    }
    
    std::cout << ">> unregisterChannel" << std::endl;
    SocketPtr sockptr = cnnl->getChannelSocket();
    channels_.erase(sockptr->getSocket());
    removeChannel(cnnl);
    displayChannelList();
    return 0;
}

bool EventsLoop::contains(const SocketPtr& sockptr) {
    if (sockptr == nullptr || !sockptr->exist() ||channels_.empty()) {
        return false;
    }

    auto iter = channels_.find(sockptr->getSocket());
    if(iter == channels_.end()) {
        return false;
    }
    return true;
}

bool EventsLoop::contains(const ChannelPtr& cnnl) {
    if(cnnl == nullptr || !cnnl->alive()) {
        return false;
    }

    auto sp = cnnl->getChannelSocket();
    return contains(sp);
}


void EventsLoop::setTimeout(const int time) {
    waitTime_ = time;
}

int EventsLoop::getEpollBase() const {
    return epfd_;
}

int EventsLoop::updateChannel(const ChannelPtr& cnptr) {

    if(ctlChannel(cnptr, EPOLL_CTL_MOD) == -1) {
        if(errno == ENOENT) {
            std::cout << ">> first add channel event" << std::endl;
            return ctlChannel(cnptr, EPOLL_CTL_ADD);
        }
        else {
            std::cout << ">> Mod channel event error: no such channel, ignore" << std::endl;
            return -1;
        }
    }
    return 0;
}

int EventsLoop::loop() {

    loop_ = true;
    while(loop_) {
        std::cout << ">> loop now" << std::endl;
        waitChannel();
    }
    return true;
}

void EventsLoop::displayChannelList() const{
    return;
    std::cout << "------------------------" << std::endl;
    for(auto &item : channels_) {
        std::cout << "socket [" << item.first << "] has channel :" << item.second << std::endl; 
    }
    std::cout << "------------------------" << std::endl;
}

int EventsLoop::waitChannel() {
 
        memset(epevents_, 0, sizeof(epevents_));
        int num = epoll_wait(epfd_, epevents_, channelNum_, waitTime_);

        ChannelPtr cnptr = nullptr;
        SocketPtr sockptr = nullptr;
        
        std::cout << ">> waited [" << num <<"] events" << std::endl;
        for(int i = 0; i < num; i++) {
            cnptr = nullptr;
            sockptr = nullptr;
            int fd = epevents_[i].data.fd;
            auto iter = channels_.find(fd);

            displayChannelList();
            if( iter == channels_.end()) {
                std::cout << ">> not find socket[" << fd <<"] events in registered list" << std::endl;
                epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, &epevents_[i]); 
                continue;
            }

            cnptr = iter->second;
            if(!cnptr) { 
                std::cout << ">> cnptr is null in registered list" << std::endl;
                unregisterChannel(cnptr);
                continue;
            }
            else {
                sockptr = cnptr->getChannelSocket();
                if(!sockptr || !sockptr->exist()) {
                    std::cout << ">> socketptr is null in registered list" << std::endl;
                    unregisterChannel(cnptr);
                    continue;
                }
            }
            
            std::cout << ">> waited socket["<< fd << "]" << std::endl;
            if(cnptr->before_cb) {
                int ret = cnptr->before_cb(sockptr);
                if(ret != EVENT_CALLBACK_CONTINUE) {
                    continue;
                }
            }

            if((epevents_[i].events & EPOLLRDHUP) && (epevents_[i].events & EPOLLIN)) {

                std::cout<< "socket [" <<fd <<"]: RDHUP event" << std::endl;
                if (cnptr->error_cb) {
                    int ret = cnptr->error_cb(sockptr);
                    if(ret != EVENT_CALLBACK_CONTINUE) {
                        continue;
                    }
                }
            }

            if(epevents_[i].events & EPOLLIN) {
                std::cout<< "socket [" <<fd <<"]: EPOLLIN event" << std::endl;
                if(cnptr->readable_cb) {
                    int ret = cnptr->readable_cb(sockptr);
                    if(ret != EVENT_CALLBACK_CONTINUE) {
                       continue; 
                    }
                }
            }

            if(epevents_[i].events & EPOLLOUT) {
                std::cout<< "socket [" <<fd <<"]: EPOLLOUT event" << std::endl;
                if(cnptr->writeable_cb) {
                    int ret = cnptr->writeable_cb(sockptr);
                    if(ret != EVENT_CALLBACK_CONTINUE) {
                        continue;
                    }
                }
            }

            if(epevents_[i].events & EPOLLHUP) {
                std::cout<< "socket [" <<fd <<"]: HUP event" << std::endl;
                if (cnptr->error_cb) {
                    int ret = cnptr->error_cb(sockptr);
                    if(ret != EVENT_CALLBACK_CONTINUE) {
                        continue;
                    }
                }
            }

            if(epevents_[i].events & EPOLLERR) {
                std::cout<< "socket [" <<fd <<"]: ERROR event" << std::endl;
                 if (cnptr->error_cb) {
                    int ret = cnptr->error_cb(sockptr);
                    if(ret != EVENT_CALLBACK_CONTINUE) {
                        continue;
                    }
                }
            }

            if(cnptr->after_cb) {
                cnptr->after_cb(sockptr);
            }
        }
        return num;
}

int EventsLoop::createEpollBase() {
    if(epfd_ == -1) {
        epfd_ = epoll_create(channelNum_);
    }
    return 0;
}

int EventsLoop::ctlChannel(const ChannelPtr& cnptr, const int op) {
    evf_t evts = transFlag(cnptr);
    epoll_event epev;
    auto sp = cnptr->getChannelSocket();
    if(!sp) { return -1; }
    socket_t sockfd = sp->getSocket();
    epev.data.fd = sockfd;
    epev.events = evts;
    return epoll_ctl(epfd_, op, sockfd, &epev); 
}

int EventsLoop::addChannel(const ChannelPtr& cnptr) {
    auto sp = cnptr->getChannelSocket();
    if(!sp) { return -1; }

    int fd = sp->getSocket();

    std::cout<< "socket [" <<fd <<"] add event" << std::endl;
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

int EventsLoop::removeChannel(const ChannelPtr& cnptr) {
    auto sp = cnptr->getChannelSocket();
    if(!sp) { return -1; }

    int fd = sp->getSocket();
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

evf_t EventsLoop::transFlag(const ChannelPtr& cnptr) {
    evf_t evts = 0;
    if(cnptr->events.readFlag) {
        evts |= EPOLLIN;
        std::cout<<" >> enable read event" << std::endl;
    }
    if(cnptr->events.writeFlag) {
        evts |= EPOLLOUT;
        std::cout<<" >> enable write event" << std::endl;
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

}
