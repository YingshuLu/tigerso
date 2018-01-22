#ifndef TS_HTTP_HTTPRESPONSE_H_
#define TS_HTTP_HTTPRESPONSE_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include "http/HttpMessage.h"

namespace tigerso {

//Http response
class HttpResponse: public HttpMessage {
public:
    HttpResponse();
    int         getStatuscode();
    std::string getDesc();
    void setStatuscode(int code);
    void setDesc(const std::string& desc);
    std::string& getHeader();
    std::string toString();
    void clear();

public:
    //Response string
    static const std::string OK;
    static const std::string BAD_REQUEST; 
    static const std::string NOT_FOUND;
    static const std::string FORBIDDEN;

private:
    int         statuscode_;
    std::string desc_;
};

} //namespace tigerso::http

#endif // TS_HTTP_HTTPRESPONSE_H_
