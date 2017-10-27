#ifndef TS_HTTP_HTTPMESSAGE_H_
#define TS_HTTP_HTTPMESSAGE_H_

#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <memory>
#include "core/BaseClass.h"
#include "core/tigerso.h"

extern "C" {
#include "http/lib/http_parser.h"
}
namespace tigerso::http {

typedef int http_role_t;
typedef std::vector<std::pair<std::string,std::string>> headers_t;

const http_role_t HTTP_ROLE_UINIT = -1;
const http_role_t HTTP_ROLE_REQUEST = 0;
const http_role_t HTTP_ROLE_RESPONSE = 1;

const int HTTP_INSPECTION_CONTINUE = 0;
const int HTTP_INSPECTION_BLOCK = -1;
const int HTTP_INSPECTION_MODIFIED = 1;

class HttpHelper {
public:
    HttpHelper(){}
    static bool isVaildResponseStatusCode(const int code) {
       return (RESPONSE_STATUS_MAP.find(code) != RESPONSE_STATUS_MAP.end());
    }

    static std::string getResponseStatusDesc(const int code) {
        auto iter = RESPONSE_STATUS_MAP.find(code);
        if( iter == RESPONSE_STATUS_MAP.end() ) {
            return std::string("");
        }
        return iter->second;
    }

private:
    static const std::map<int, std::string> RESPONSE_STATUS_MAP;
};

//Abstract http base message
class HttpMessage {
public:
    //Request & Response line iterms
    static const std::string METHOD;
    static const std::string URL;
    static const std::string VERSION;
    static const std::string STATUSCODE;
    static const std::string DESC;

    virtual std::string getMethod(){ return ""; };
    virtual std::string getUrl(){ return ""; };
    virtual int         getStatuscode(){ return 0; };
    virtual std::string getDesc(){ return ""; };
    virtual std::string getVersion() { return version_; };
    virtual std::string getValueByHeader(const std::string& header) {
        for ( auto iter = headers_.begin(); iter != headers_.end(); iter++ ) {
            if( strcasecmp(header.c_str(), iter->first.c_str()) == 0) {
                return iter->second;
            }
        }
        return ""; 
    }
    virtual std::string& getBody() { return body_; }
    
    virtual void setMethod(const std::string& method){};
    virtual void setUrl(const std::string& Url){};
    virtual void setStatuscode(int){};
    virtual void setDesc(const std::string&){};
    virtual void setVersion(const std::string& verison) { version_ = verison; }
    virtual void setValueByHeader(const std::string& header, const std::string& value) {
        for ( auto iter = headers_.begin(); iter != headers_.end(); iter++ ) {
            if( strcasecmp(header.c_str(), iter->first.c_str()) == 0) {
                iter->second = value;
                return;
            }
        }
        headers_.push_back(std::make_pair(header,value));
        return;
    }

    virtual void setBody(const std::string& body) {
        body_.append(body);
    }

    virtual void removeHeader(const std::string& header) {
        for ( auto iter = headers_.begin(); ; iter++ ) {
            if( iter != headers_.end() && strcasecmp(header.c_str(), iter->first.c_str()) == 0) {
                iter = headers_.erase(iter);
            }

            if(iter == headers_.end()) {
                break;
            }
        }
        return;
    }

    virtual void removeRepeatHeader() {
        return;
    }

    virtual void markTrade() {
        std::string val = getValueByHeader("Via");
        if (!val.empty()) { val.append(", "); }
        val.append("tigerso/");
        val.append(core::VERSION);
        headers_.push_back(std::make_pair("Via", val));
    }

    virtual void appendHeader(std::string header, std::string value) {
        for ( auto iter = headers_.begin(); iter != headers_.end(); iter++ ) {
            if( strcasecmp(header.c_str(), iter->first.c_str()) == 0) {
                if (!iter->second.empty()) { iter->second.append(", "); }
                iter->second.append(value);
                return;
            }
        }
        headers_.push_back(std::make_pair(header,value));
        return;
    }

    virtual std::string toString() = 0;
    virtual http_role_t getRole() { return role_; }
    virtual void clear() {
        version_ = "HTTP/1.1";
        headers_.clear();
        body_.clear();
    }
    
    // Temp variant for http parser
    std::string header_field;

protected:
    http_role_t role_ = HTTP_ROLE_UINIT;
    std::string version_ = "HTTP/1.1";
    headers_t headers_;
    std::string body_;
};

//Http request
class HttpRequest: public HttpMessage {
public:
    HttpRequest() { role_ = HTTP_ROLE_REQUEST; }
    
    std::string getMethod() { return method_; }
    std::string getUrl() { return url_; }
    void setMethod(const std::string& method) { method_ = method; }
    void setUrl(const std::string& url) { url_ = url; }

    std::string toString() {
        if(method_.empty() || url_.empty()) {
            return std::string("");
        }
        std::string request = method_ + " " + url_ + " " + version_ + "\r\n";
        auto iter = headers_.begin();
        while (iter != headers_.end()) {
            request.append(iter->first + ": " + iter->second + "\r\n");
            ++iter;
        }
        request.append("\r\n");
        if(!body_.empty()) {
            request.append(body_ + "\r\n");
        }
        return request;
    }

    void clear() {
        method_.clear();
        url_.clear();
        HttpMessage::clear();
    }

    std::string getHostPort() {
        if(!port_.empty()) { return port_; }

        std::string url = url_;
        std::string::size_type pos = url.find("http://");
        if(pos != std::string::npos) { url = url.substr(pos+7); }
        else {
            pos = url.find("https://");
            if(pos != std::string::npos) { url = url.substr(pos+8); }
        }
        
        pos = url.find("/");
        if (pos != std::string::npos) {
            url = url.substr(0,pos);
            std::string::size_type label = url.find(":");
            if(label != std::string::npos) {
                port_ = url.substr(label+1);
                return port_;
            }
        }
        
        port_ = "80";
        const std::string& host = getValueByHeader("host");
        if(!host.empty()) {
            pos = host.find(":");
            if(pos != std::string::npos) {
                port_ = host.substr(pos+1);
            }
        }
        return port_;
    }

private:
    std::string method_;
    std::string url_;
    std::string port_;
};

//Http response
class HttpResponse: public HttpMessage {
public:
    HttpResponse() { role_ = HTTP_ROLE_RESPONSE; }
    
    int         getStatuscode() { return statuscode_; }
    std::string getDesc() { return desc_; }
    void setStatuscode(int code) { 
        statuscode_ = code;
        std::string desc = HttpHelper::getResponseStatusDesc(code);
        if (!desc.empty()) {
            setDesc(desc);
        }
    }
    void setDesc(const std::string& desc) { desc_ = desc; }

    std::string toString() {
        if(statuscode_ == 0 || desc_.empty() ) {
            return std::string("");
        }
        std::string response = version_ + " " + std::to_string(statuscode_) + " " + desc_ + "\r\n";
        auto iter = headers_.begin();
        while (iter != headers_.end()) {
            response.append(iter->first + ": " + iter->second + "\r\n");
            ++iter;
        }
        response.append("\r\n");
        if(!body_.empty()) {
            response.append( body_ + "\r\n");
        }
        return response;
    }

    void clear() {
      statuscode_ = 0;
      desc_.clear();
      HttpMessage::clear();
    }

    //Response string
    static const std::string OK;
    static const std::string BAD_REQUEST; 
    static const std::string NOT_FOUND;
    static const std::string FORBIDDEN;
   
private:
    int         statuscode_;
    std::string desc_;
};

//Http parser
class HttpParser: public core::nocopyable {
public:
    HttpParser(): buffer_(nullptr), length_(0) {}
    int parse(const char*, size_t, HttpMessage&);
    int parse(const std::string&, HttpMessage&);

private:
    // Callbacks for http data parser 
    static int on_message_begin(http_parser*);
    static int on_message_complete(http_parser*);
    static int on_headers_complete(http_parser*);
    static int on_url(http_parser*, const char*, size_t);
    static int on_status(http_parser*, const char*, size_t);
    static int on_header_field(http_parser*, const char*, size_t);
    static int on_header_value(http_parser*, const char*, size_t);
    static int on_body(http_parser*, const char*, size_t);
    int initParser(HttpMessage&);

    const char* buffer_;
    size_t length_;
    http_parser parser_;
    http_parser_settings settings_;
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

} //namespace tigerso::http

#endif // TS_HTTP_HTTPMESSAGE_H_
