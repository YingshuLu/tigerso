#include <iostream>
#include <memory>
#include "net/Channel.h"
#include "net/EventsLoop.h"

namespace tigerso::net {

 // quick set event
bool Channel::enableReadEvent() {
    events.readFlag = true;
    update();
    return true;
}

bool Channel::disableReadEvent() {
    events.readFlag = false;
    update();
    return true;
}

bool Channel::enableWriteEvent() {
    events.writeFlag = true;
    update();
    return true;
}

bool Channel::disableWriteEvent() {
    events.writeFlag = false;
    update();
    return true;
}

bool Channel::alive() {
    if( sock_.lock() ) {
        return true;
    }
    return false;
}

void Channel::remove() {
     disableAllEvent();
     loop_.unregisterChannel(getPtr());
}

void Channel::reset() {
    resetFlag();
}

std::shared_ptr<Channel> Channel::getPtr() {
    std::shared_ptr<Channel> cnptr = shared_from_this();
    return cnptr;
}

int Channel::setEvents(bool readable, bool writeable, bool edge, bool keep) {
    events.readFlag = readable;
    events.writeFlag = writeable;
    events.hupFlag = true;
    events.edgeFlag = edge;
    events.keepFlag = keep;
    return 0;
}

std::shared_ptr<Socket> Channel::getChannelSocket() const {
    auto ptr = sock_.lock();
    if( !ptr ) {
        return nullptr;
    }
    return ptr;
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

bool Channel::disableAllEvent() {
    disableReadEvent();
    disableWriteEvent();
    return 0;
}

void Channel::resetFlag() {
    setEvents(false, false, true, true);
}

bool Channel::update() {

    /*
       if (!events.readFlag && !events.writeFlag) {
       loop_.removeEvent(*this);
       }
       else {
       loop_.updateEvent(*this);
       }
       */
    ChannelPtr cnptr = getPtr();
    if (!cnptr) {
        std::cout << "Channel ptr is null" << std::endl;
        return false;
    }
    std::cout << "share ptr: " << cnptr << std::endl;
    loop_.updateChannel(getPtr());
    return true;
}

}
