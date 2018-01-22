#ifndef TS_HTTP_HTTPFILTER_H_
#define TS_HTTP_HTTPFILTER_H_

namespace tigerso {

class HttpRequest;
class HttpResponse;

class HttpFilter {

public:
    void init();
    void destory();
    void doFilter(HttpRequest&, HttpResponse&)
    
};

} //namespace tigerso

#endif
