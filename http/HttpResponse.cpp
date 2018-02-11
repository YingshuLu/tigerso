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
#include "http/HttpResponse.h"

namespace tigerso {

//Http response
HttpResponse::HttpResponse() { role_ = HTTP_ROLE_RESPONSE; }

int HttpResponse::getStatuscode() { return statuscode_; }
std::string HttpResponse::getDesc() { return desc_; }
void HttpResponse::setStatuscode(int code) { 
    statuscode_ = code;
    std::string desc = HttpHelper::getResponseStatusDesc(code);
    if (!desc.empty()) {
        setDesc(desc);
    }
}

void HttpResponse::setDesc(const std::string& desc) { desc_ = desc; }

void HttpResponse::shouldNoBody() { parser_.labelNoBody(); }

std::string& HttpResponse::getHeader() { 
    if(statuscode_ == 0 || desc_.empty() ) {
        headstr_ = "";
        return headstr_;
    }

    headstr_ = version_ + " " + std::to_string(statuscode_) + " " + desc_ + "\r\n";
    auto iter = headers_.begin();
    while (iter != headers_.end()) {
        headstr_.append(iter->first + ": " + iter->second + "\r\n");
        ++iter;
    }
    headstr_.append("\r\n");
    return headstr_;
}

std::string HttpResponse::toString() {
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
    /*
    if(!body_.empty()) {
        response.append( body_ + "\r\n");
    }
    */
    return response;
}

class HttpMessage;
void HttpResponse::clear() {
  statuscode_ = 0;
  desc_.clear();
  HttpMessage::clear();
}

bool HttpResponse::unlinkAfterWrite(bool unlink) { return body_.unlinkAfterSend(unlink); }

//Response string
const std::string HttpResponse::OK = "HTTP/1.1 200 OK\r\nserver: tigerso/" + core::VERSION + "\r\n\r\n";
const std::string HttpResponse::BAD_REQUEST = "HTTP/1.1 400 Bad Request\r\nserver: tigerso/" + core::VERSION + "\r\n\r\n";
const std::string HttpResponse::NOT_FOUND = "HTTP/1.1 404 Not Found\r\nserver: tigerso/" + core::VERSION + "\r\n\r\n";
const std::string HttpResponse::FORBIDDEN = "HTTP/1.1 403 Forbidden\r\nserver: tigerso/" + core::VERSION + "\r\n\r\n";


} //namespace tigerso::http

