#include <stdlib.h>
#include <functional>
#include "http/HttpParser.h"
#include "core/File.h"
#include "core/Logging.h"

namespace tigerso {

#define PARSER_TO_MESSAGE(parser) (static_cast<HttpParser*>(parser->data))->message_

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
    skipbody_ = false;
    bigfile_ = false;
}

int HttpParser::on_message_begin(http_parser* parser) {
    HttpParser* hptr = static_cast<HttpParser*>(parser->data);
    hptr->setParseState(PARSE_HEADER_NEED_MORE_DATA);
    return 0;
}

int HttpParser::on_message_complete(http_parser* parser) {
    HttpParser* hp = static_cast<HttpParser*>(parser->data);
    hp->setParseState(PARSE_COMPLETE);
    return 0;
}

int HttpParser::on_url(http_parser* parser, const char* at, size_t len) {
    HttpMessage* pmessage = PARSER_TO_MESSAGE(parser);
    pmessage->setUrl(std::string(at, len));
    return 0;
}

int HttpParser::on_status(http_parser* parser, const char* at, size_t len) {
    HttpMessage* pmessage = PARSER_TO_MESSAGE(parser);
    pmessage->setDesc(std::string(at, len));
    return 0;
}

int HttpParser::on_header_field(http_parser* parser, const char* at, size_t len) {
    HttpMessage* pmessage = PARSER_TO_MESSAGE(parser);
    pmessage->header_field.assign(at, len);
    return 0;
}

int HttpParser::on_header_value(http_parser* parser, const char* at, size_t len) {
    HttpMessage* pmessage = PARSER_TO_MESSAGE(parser);
    pmessage->setValueByHeader(pmessage->header_field, std::string(at, len));
    return 0;
}

int HttpParser::on_headers_complete(http_parser* parser) {
    using namespace tigerso::core;
    HttpMessage* pmessage = PARSER_TO_MESSAGE(parser);
    if(pmessage->getRole() == HTTP_ROLE_REQUEST) { pmessage->setMethod(http_method_str((http_method)parser->method)); }
    else { pmessage->setStatuscode(parser->status_code); }
    
    if( !(parser->http_major > 0 && parser->http_minor > 0 ) ) { pmessage->setVersion("HTTP/1.0"); }
    HttpParser* hptr = static_cast<HttpParser*>(parser->data);
    hptr->setParseState(PARSE_HEADER_COMPLETE);
    DBG_LOG("Http Parser get complete header:\n%s", pmessage->getHeader().c_str());

    //HEAD method request, so null body in response
    if(hptr->skipBody()) { return 1; }

    int content_length = 0;
    std::string cntlen = pmessage->getValueByHeader("content-length");
    if(!cntlen.empty()) {
        content_length = ::atoi(cntlen.c_str());
    }

    if(content_length >= FILE_BIG_CONTENT) {
        hptr->labelBigFile();
    }
    return 0;
}

int HttpParser::on_body(http_parser* parser, const char* at, size_t len) {
    using namespace tigerso::core;
    HttpMessage* pmessage = PARSER_TO_MESSAGE(parser);
    HttpParser* hptr = static_cast<HttpParser*>(parser->data);

    //big file: truched transfer-encoding, not copy to response, need tunnel
    if(!hptr->isBigFile()) { 
        if(pmessage->getBody().size() >= FILE_BIG_CONTENT) { hptr->labelBigFile(); }
        if(!hptr->isBigFile()) { pmessage->setBody(std::string(at, len)); }
    }

    hptr->setParseState(PARSE_BODY_NEED_MORE_DATA);

    return 0;
}

}//namespace tigerso::http
