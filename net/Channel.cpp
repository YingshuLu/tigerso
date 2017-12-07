#include <iostream>
#include <memory>
#include "net/Channel.h"
#include "net/EventsLoop.h"
#include "core/Logging.h"

namespace tigerso::net {

 // quick set event
bool Channel::enableReadEvent() {
    if(events.readFlag) { return true; }
    events.readFlag = true;
    update();
    DBG_LOG("enable [%d] read event", sockfd);
    return true;
}

bool Channel::disableReadEvent() {
    if(!events.readFlag) { return true; }
    events.readFlag = false;
    update();
    DBG_LOG("disable [%d] read event", sockfd);
    return true;
}

bool Channel::enableWriteEvent() {
    if(events.writeFlag) { return true; }
    events.writeFlag = true;
    update();
    DBG_LOG("enable [%d] write event", sockfd);
    return true;
}

bool Channel::disableWriteEvent() {
    if(!events.writeFlag) { return true; }
    events.writeFlag = false;
    update();
    DBG_LOG("disable [%d] write event", sockfd);
    return true;
}

void Channel::remove() {
     disableAllEvent();
     loop_.unregisterChannel(*sock_);
}

int Channel::setEvents(bool readable, bool writeable, bool edge, bool keep) {
    events.readFlag = readable;
    events.writeFlag = writeable;
    events.hupFlag = true;
    events.edgeFlag = edge;
    events.keepFlag = keep;
    return 0;
}

Socket* Channel::getSocketPtr() const {
    return sock_;
}

EventFunc Channel::setBeforeCallback(EventFunc func) {
    before_cb = func;
    return func;
}

EventFunc Channel::setAfterCallback(EventFunc func) {
    after_cb = func;
    return func;
}

EventFunc Channel::setReadCallback(EventFunc func) {
    readable_cb = func;
    return func;
}

EventFunc Channel::setWriteCallback(EventFunc func) {
    writeable_cb = func;
    return func;
}

EventFunc Channel::setErrorCallback(EventFunc func) {
    error_cb = func;
    return func;
}

EventFunc Channel::setRdhupCallback(EventFunc func) {
    error_cb = func;
    return func;
}

bool Channel::disableAllEvent() {
    disableReadEvent();
    disableWriteEvent();
    return 0;
}

void Channel::resetFlag() {
    setEvents(false, false, true, true);
}

bool Channel::update() {
    return loop_.updateChannel(this) == 0;
}

}
