#ifndef TS_HTTP_HTTPREQUEST_H_
#define TS_HTTP_HTTPREQUEST_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include "http/HttpMessage.h"

namespace tigerso {

//Http request
class HttpRequest: public HttpMessage {
public:
    HttpRequest();    
    std::string getMethod();
    std::string getUrl();
    void setMethod(const std::string& method);
    void setUrl(const std::string& url);
    std::string& getHeader();
    std::string toString();
    void clear();
    std::string getHost();
    std::string getHostPort();
    //std::string& getBodyFileName();

private:
    std::string method_;
    std::string url_;
    std::string host_;
    std::string port_;
};

} //namespace tigerso::http

#endif // TS_HTTP_HTTPREQUEST_H_
