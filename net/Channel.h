#ifndef TS_NET_CHANNEL_H_
#define TS_NET_CHANNEL_H_

#include <sys/epoll.h>
#include <errno.h>
#include <string.h>
#include <vector>
#include <functional>
#include <map>
#include <memory>
#include "core/Logging.h"
#include "core/BaseClass.h"
#include "net/Socket.h"

namespace tigerso {

const int EVENT_CALLBACK_CONTINUE = 0;
const int EVENT_CALLBACK_BREAK = 1;
const int EVENT_CALLBACK_DROPWAITED = 2;

typedef std::function<int(Socket&)> EventFunc; 
int handle_default_error(Socket);
class EventsLoop;
typedef unsigned int evf_t;

class Channel: public nocopyable {
friend class EventsLoop;
public:
    Channel(EventsLoop& loop, Socket& sock)
        :loop_ (loop),
        sock_(&sock),
        readable_cb(nullptr),
        writeable_cb(nullptr),
        error_cb(nullptr) {
        resetFlag();
        sockfd = sock_->getSocket();
    }

public:
    // quick set event
    bool enableReadEvent();
    bool disableReadEvent();
    bool enableWriteEvent();
    bool disableWriteEvent();
    bool disableAllEvent();
    void remove();
    int setEvents(bool readable, bool writeable, bool edge = true, bool keep = true);
    Socket* getSocketPtr() const;
    EventFunc setBeforeCallback(EventFunc);
    EventFunc setAfterCallback(EventFunc);
    EventFunc setReadCallback(EventFunc);
    EventFunc setWriteCallback(EventFunc);
    EventFunc setErrorCallback(EventFunc);
    EventFunc setRdhupCallback(EventFunc);
    EventFunc setTimeoutCallback(EventFunc);

    EventFunc getTimeoutCallback();

    //Mark this channel
    int sockfd = -1;
private:
    bool update();
    void resetFlag();
    //need wait event flag
    struct {
        bool readFlag;
        bool writeFlag;
        bool hupFlag;
        bool edgeFlag;
        bool keepFlag;
    } events;

    Socket* sock_;
    
    EventsLoop& loop_;
    EventFunc before_cb = nullptr;
    EventFunc after_cb = nullptr;
    EventFunc readable_cb = nullptr;
    EventFunc writeable_cb = nullptr;
    EventFunc error_cb = nullptr;
    EventFunc rdhup_cb = nullptr;
    EventFunc timeout_cb = nullptr;
};

}//namespace tigerso::net

#endif //TS_NET_CHANNEL_H_
