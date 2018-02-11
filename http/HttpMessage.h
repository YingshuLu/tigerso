#ifndef TS_HTTP_HTTPMESSAGE_H_
#define TS_HTTP_HTTPMESSAGE_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <memory>
#include "http/HttpBodyFile.h"
#include "core/BaseClass.h"
#include "core/tigerso.h"
#include "core/File.h"
#include "core/SysUtil.h"
#include "util/FileTypeDetector.h"
#include "http/HttpParser.h"

namespace tigerso {

typedef int http_role_t;
typedef std::vector<std::pair<std::string,std::string>> headers_t;

const http_role_t HTTP_ROLE_UINIT = -1;
const http_role_t HTTP_ROLE_REQUEST = 0;
const http_role_t HTTP_ROLE_RESPONSE = 1;

const int HTTP_INSPECTION_CONTINUE = 0;
const int HTTP_INSPECTION_BLOCK = -1;
const int HTTP_INSPECTION_MODIFIED = 1;

class HttpResponse;
class HttpRequest;
class HttpHelper {
public:
    HttpHelper(){}
    static int prepare200Response(HttpResponse& response);
    static int prepare400Response(HttpResponse& response);
    static int prepare403Response(HttpResponse& response);
    static int prepare503Response(HttpResponse& response);
    static int prepare504Response(HttpResponse& response);
    static int prepareDNSErrorResponse(HttpResponse& response);
    static bool isVaildResponseStatusCode(const int code);
    static std::string getResponseStatusDesc(const int code);

// html file name
public:
    static std::string response403html;
    static std::string response503html;

private:
    static const std::map<int, std::string> RESPONSE_STATUS_MAP;
};

//Abstract http base message
class HttpMessage {
public:
    virtual std::string getMethod() { return ""; }
    virtual std::string getUrl() { return ""; }
    virtual int         getStatuscode() { return -1; }
    virtual std::string getDesc() { return ""; }
    virtual void setMethod(const std::string& method) {}
    virtual void setUrl(const std::string& Url){}
    virtual void setStatuscode(int) {}
    virtual void setDesc(const std::string&) {}
    virtual std::string getHost() { return std::string(""); }
    virtual std::string getHostPort() { return std::string(""); } 

    virtual void clear();    
    virtual std::string& getHeader() = 0;
    virtual std::string toString() = 0;

    // Temp variant for http parser
    std::string header_field;
    int  getContentLength();    
    HttpBodyFile* getBody();
    std::string getVersion();
    std::string getValueByHeader(const std::string& header);
    void setVersion(const std::string& verison);
    void setContentLength(const unsigned int);
    void setContentType(const std::string&);
    void setCookie(const std::string&);
    void setKeepAlive(bool);
    void setChunkedTransfer(bool);
    void setValueByHeader(const std::string& header, const std::string& value);
    void setBody(const std::string& body);
    void appendBody(const char* buf, size_t len);
    void removeHeader(const std::string& header);
    void removeRepeatHeader();
    void markTrade();
    void appendHeader(std::string header, std::string value);
    http_role_t getRole();
    std::string getBodyFileName();
    int setBodyFileName(const std::string& fn);
    bool    isKeepAlive();
    bool    isChunked();
    virtual ~HttpMessage();

public:
    int parse(const char*, const size_t&, int*);
    HttpParser* getParserPtr();
    void setHeaderCompletedHandle(HttpParserCallback);
    void setBodyCompletedHandle(HttpParserCallback);
    void tunnelBody();
    bool isTunnelBody();
    bool needMoreData();
    bool isHeaderCompleted();

protected:
    const char* detectMIMEType(const std::string&);

protected:
    http_role_t role_ = HTTP_ROLE_UINIT;
    std::string version_ = "HTTP/1.1";
    headers_t headers_;
    std::string headstr_;
    HttpBodyFile body_;
    std::string bodyname_;
    HttpParser parser_;

private:
    FileTypeDetector MIMETyper_;
};


//Http Role inspection
typedef int (*httpInspectCallback)(const std::weak_ptr<HttpMessage>&);
class HttpInSpection {
public:
    int Inspect(const std::string& header, const std::weak_ptr<HttpMessage>& wptr);
    void Register(const std::string& header, httpInspectCallback callback, http_role_t role);
    void Unregister(const std::string& header, http_role_t role);

private:
    std::map<std::string, httpInspectCallback> requestCallbacks_;
    std::map<std::string, httpInspectCallback> responseCallbacks_;
};

} //namespace tigerso

#endif // TS_HTTP_HTTPMESSAGE_H_
