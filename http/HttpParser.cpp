#include <stdlib.h>
#include <functional>
#include "http/HttpParser.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "core/File.h"
#include "core/Logging.h"

namespace tigerso {

#define HTTP_PARSER(parser) (static_cast<HttpParser*>(parser->data))
//#define PARSER_TO_MESSAGE(parser) (static_cast<HttpParser*>(parser->data))->message_
#define PARSER_TO_MESSAGE(parser) HTTP_PARSER(parser)->message_

int on_message_begin(http_parser* parser) {
    //HttpParser* hptr = static_cast<HttpParser*>(parser->data);
    HttpParser* hptr = HTTP_PARSER(parser);
    hptr->setParseState(PARSE_HEADER_NEED_MORE_DATA);
    return 0;
}

int on_message_complete(http_parser* parser) {
    HttpParser* hp = HTTP_PARSER(parser);
    HttpMessage* pmessage = PARSER_TO_MESSAGE(parser);
    pmessage->getBody()->closeFile();
    hp->setParseState(PARSE_COMPLETE);
    if(hp->bodyCompletedHandle_) { hp->bodyCompletedHandle_(*pmessage); }
    return 0;
}

int on_url(http_parser* parser, const char* at, size_t len) {
    HttpMessage* pmessage = PARSER_TO_MESSAGE(parser);
    pmessage->setUrl(std::string(at, len));
    return 0;
}

int on_status(http_parser* parser, const char* at, size_t len) {
    HttpMessage* pmessage = PARSER_TO_MESSAGE(parser);
    pmessage->setDesc(std::string(at, len));
    return 0;
}

int on_header_field(http_parser* parser, const char* at, size_t len) {
    HttpMessage* pmessage = PARSER_TO_MESSAGE(parser);
    pmessage->header_field.assign(at, len);
    return 0;
}

int on_header_value(http_parser* parser, const char* at, size_t len) {
    HttpMessage* pmessage = PARSER_TO_MESSAGE(parser);
    pmessage->setValueByHeader(pmessage->header_field, std::string(at, len));
    return 0;
}

int on_headers_complete(http_parser* parser) {
    using namespace tigerso::core;
    HttpMessage* pmessage = PARSER_TO_MESSAGE(parser);
    HttpParser* hptr = HTTP_PARSER(parser); 

    if(pmessage->getRole() == HTTP_ROLE_REQUEST) {
        pmessage->setMethod(http_method_str((http_method)parser->method));
    }
    else { pmessage->setStatuscode(parser->status_code); }
    
    //set http version
    if( !(parser->http_major > 0 && parser->http_minor > 0 ) ) { pmessage->setVersion("HTTP/1.0"); }

    hptr->setParseState(PARSE_HEADER_COMPLETE);
    DBG_LOG("Http Parser get complete header:\n%s", pmessage->getHeader().c_str());

    //call header complete callback
    if(hptr->headerCompletedHandle_) {
        hptr->headerCompletedHandle_(*pmessage);
    }

    //HEAD method request, so null body in response
    if(hptr->isNoBody()) { return 1; }

    int content_length = 0;
    std::string cntlen = pmessage->getValueByHeader("content-length");
    //Content length has higher priority
    if(!cntlen.empty()) {
        content_length = ::atoi(cntlen.c_str());
        if(content_length >= FILE_BIG_CONTENT || pmessage->getRole() == HTTP_ROLE_REQUEST) {
            hptr->labelTunnelBody();
        }
        return 0;
    }

    std::string mime_type = pmessage->getValueByHeader("content-type");
    if(mime_type.empty()) {
        mime_type = pmessage->getValueByHeader("mime-type");
        if(!mime_type.empty()) {
            std::size_t found = mime_type.find_first_of("/");
            if(found != std::string::npos) {
                mime_type = mime_type.substr(0, found -1);
            }
        }
    }

    //only interest on text & image file
    if(strcasecmp(mime_type.c_str(), "text") || strcasecmp(mime_type.c_str(), "image")) {
        hptr->labelTunnelBody();
        return 0;
    }

    if(http_parser_is_chunked(parser)) {
        DBG_LOG("Detect chunked transfer-encoding");
        pmessage->getBody()->chunked = true;
    }
    return 0;
}

int on_body(http_parser* parser, const char* at, size_t len) {
    using namespace tigerso::core;
    HttpMessage* pmessage = PARSER_TO_MESSAGE(parser);
    HttpParser* hptr = HTTP_PARSER(parser);

    //big file: truched transfer-encoding, not copy to response, need tunnel
    if(!hptr->isTunnelBody()) { 
        if(pmessage->getBody()->size() >= FILE_BIG_CONTENT) { hptr->labelTunnelBody(); }
        if(!hptr->isTunnelBody()) { pmessage->appendBody(at, len); }
    }

    hptr->setParseState(PARSE_BODY_NEED_MORE_DATA);
    return 0;
}

int HttpParser::initParser(HttpMessage& message) {
    if(buffer_ == nullptr || length_ <= 0) {
        return -1;
    }

    if(IS_HTTPPARSER_INITED(state_)) { return 0; }

    http_parser_type parser_type;
    int retcode = 0;

    if(message.getRole() == HTTP_ROLE_REQUEST) { parser_type = HTTP_REQUEST; }
    else if (message.getRole() == HTTP_ROLE_RESPONSE) { parser_type = HTTP_RESPONSE; }
    else { retcode = -1; }

    if (retcode != 0) { return retcode; }

    http_parser_init(&parser_, parser_type);
    http_parser_settings_init(&settings_);
    parser_.data = static_cast<void*>(this);

    settings_.on_message_begin = on_message_begin;
    settings_.on_message_complete = on_message_complete;
    settings_.on_headers_complete = on_headers_complete;

    settings_.on_url = on_url;
    settings_.on_status = on_status;
    settings_.on_header_field = on_header_field;
    settings_.on_header_value = on_header_value;
    settings_.on_body = on_body;

    state_ = PARSE_INIT;

    return 0;
}

int HttpParser::parse(const char* buf,size_t len, HttpMessage& message) {
    buffer_ = buf;
    length_ = len;
    message_ = &message;
    if( initParser(message) != 0 ){ return -1; }
    parsedn_ = http_parser_execute(&parser_, &settings_, buffer_, length_);
    return parsedn_;
}

int HttpParser::parse(const std::string& buffer, HttpMessage& message) {
    return parse(buffer.c_str(), buffer.length(), message);
}

void HttpParser::reset() {
    buffer_ = nullptr;
    length_ = 0;
    parsedn_ = 0;
    state_ = PARSE_UNINIT;
    message_ = nullptr;
    nobody_ = false;
    tunnelbody_ = false;
    headerCompletedHandle_ = nullptr;
    bodyCompletedHandle_ = nullptr;
}



}//namespace tigerso::http
