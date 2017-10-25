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
#include "net/Socket.h"

namespace tigerso::net {

const int EVENT_CALLBACK_CONTINUE = 0;
const int EVENT_CALLBACK_BREAK = 1;

typedef std::function<int(SocketPtr&)> EventFunc; 
int handle_default_error(SocketPtr);
class EventsLoop;
typedef unsigned int evf_t;

class Channel: public std::enable_shared_from_this<Channel> {
friend class EventsLoop;
public:
    Channel(EventsLoop& loop, const std::shared_ptr<Socket>& sock)
        :loop_ (loop),
        sock_(sock),
        readable_cb(nullptr),
        writeable_cb(nullptr),
        error_cb(nullptr) {
        resetFlag();
    }

public:
    // quick set event
    bool enableReadEvent();
    bool disableReadEvent();
    bool enableWriteEvent();
    bool disableWriteEvent();
    bool disableAllEvent();
    bool alive();
    void remove();
    void reset();
    std::shared_ptr<Channel> getPtr(); 
    int setEvents(bool readable, bool writeable, bool edge = true, bool keep = true);
    std::shared_ptr<Socket> getChannelSocket() const;
    EventFunc setBeforeCallback(EventFunc);
    EventFunc setAfterCallback(EventFunc);
    EventFunc setReadCallback(EventFunc);
    EventFunc setWriteCallback(EventFunc);
    EventFunc setErrorCallback(EventFunc);

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

    const std::weak_ptr<Socket> sock_;
    EventsLoop& loop_;
    EventFunc before_cb = nullptr;
    EventFunc after_cb = nullptr;
    EventFunc readable_cb = nullptr;
    EventFunc writeable_cb = nullptr;
    EventFunc error_cb = nullptr;
 
};

}//namespace tigerso::net

#endif //TS_NET_CHANNEL_H_
