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
#include "http/HttpRequest.h"

namespace tigerso {

HttpRequest::HttpRequest() { role_ = HTTP_ROLE_REQUEST; }
std::string HttpRequest::getMethod() { return method_; }
std::string HttpRequest::getUrl() { return url_; }
void HttpRequest::setMethod(const std::string& method) { method_ = method; }
void HttpRequest::setUrl(const std::string& url) { url_ = url; }

std::string& HttpRequest::getHeader() {
    if(method_.empty() || url_.empty()) {
        headstr_ = "";
        return headstr_;
    }

    headstr_ = method_ + " " + url_ + " " + version_ + "\r\n";
    auto iter = headers_.begin();
    while (iter != headers_.end()) {
        headstr_.append(iter->first + ": " + iter->second + "\r\n");
        ++iter;
    }

    headstr_.append("\r\n");
    return headstr_;
}

std::string HttpRequest::toString() {
    std::string request;
    if(method_.empty() || url_.empty()) {
        return std::string("");
    }

    request = method_ + " " + url_ + " " + version_ + "\r\n";
    auto iter = headers_.begin();
    while (iter != headers_.end()) {
        request.append(iter->first + ": " + iter->second + "\r\n");
        ++iter;
    }
    
    request.append("\r\n");
    return request;
}

void HttpRequest::clear() {
    method_.clear();
    url_.clear();
    port_.clear();
    host_.clear();
    HttpMessage::clear();
}

std::string HttpRequest::getHost() {
    if(!host_.empty()) {
        return host_;
    }
    std::string host = getValueByHeader("host");
    std::string::size_type pos = host.find(":");
    if(pos != std::string::npos) {
        host = host.substr(0, pos);
    }

    host_ = host;
    return host_;
}

std::string HttpRequest::getHostPort() {
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

std::string& HttpRequest::getBodyFileName() {
    if(bodyname_.size()) {
        return bodyname_;
    }
    
    std::string filename = getValueByHeader("host");
    /*
    std::size_t found = url_.find_last_of("/");
    if(found != std::string::npos) {
        filename = url_.substr(found+1);
    }
    */
    if(filename.empty()) {
        filename = host_;
    }

    DBG_LOG("filename :%s", filename.c_str());
    bodyname_ = filename + "ToTigerso-" + SysUtil::getFormatTime("%H%M%S");
    return bodyname_;
}

} //namespace tigerso::http

