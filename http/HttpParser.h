#ifndef TS_HTTP_HTTPPARSER_H_
#define TS_HTTP_HTTPPARSER_H_

#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <memory>
#include "core/BaseClass.h"
#include "core/tigerso.h"

namespace tigerso {
/*Http parser
 *NEVER change to be parserd buf
 *Parser would store values on HTTPMessage
 */
extern "C" {
#include "http/lib/http_parser.h"
}

typedef enum{
    PARSE_UNINIT = -1,
    PARSE_ERR = 0,
    PARSE_INIT,
    PARSE_HEADER_NEED_MORE_DATA,
    PARSE_HEADER_COMPLETE,
    PARSE_BODY_NEED_MORE_DATA,
    PARSE_COMPLETE
} PARSE_STATE; 

#define IS_HTTPPARSER_INITED(state) (state > PARSE_UNINIT)
#define HP_PARSE_ERROR -1
#define HP_PARSE_BAD_REQUEST -2
#define HP_PARSE_BAD_RESPONSE -3
#define HP_PARSE_COMPLETE 0
#define HP_PARSE_NEED_MOREDATA 1
#define HP_PARSE_PIPELINE  2

class HttpMessage;

typedef std::function<int(HttpMessage&)> HttpParserCallback;
class HttpParser: public nocopyable {

friend int on_message_begin(http_parser*);
friend int on_message_complete(http_parser*);
friend int on_headers_complete(http_parser*);
friend int on_url(http_parser*, const char*, size_t);
friend int on_status(http_parser*, const char*, size_t);
friend int on_header_field(http_parser*, const char*, size_t);
friend int on_header_value(http_parser*, const char*, size_t);
friend int on_body(http_parser*, const char*, size_t);

public:
    HttpParser(): buffer_(nullptr), length_(0), parsedn_(0), state_(PARSE_UNINIT), message_(nullptr) {}
    int parse(const char*, size_t, HttpMessage&);
    int parse(const std::string&, HttpMessage&);
    inline PARSE_STATE getParseState() { return state_; }
    bool isNoBody() { return nobody_; }
    bool headerCompleted() {  return (state_ >= PARSE_HEADER_COMPLETE? true : false); }
    bool needMoreData() { return state_ != PARSE_COMPLETE; }
    const char* getStrErr() { return http_errno_description(HTTP_PARSER_ERRNO(&parser_)); }
    size_t getLastParsedSize() { return parsedn_; }
    bool isTunnelBody() { return tunnelbody_; }
    void reset();

public:
    void setHeaderCompletedHandle(HttpParserCallback callback) { headerCompletedHandle_ = callback; }
    void setBodyCompletedHandle(HttpParserCallback callback) { bodyCompletedHandle_ = callback; }
    void labelNoBody() { nobody_ = true; }
    bool labelTunnelBody() { tunnelbody_ = true; }

private:
    void setParseState(PARSE_STATE state) { state_ = state; }
    int initParser(HttpMessage&);

private:
    const char* buffer_;
    size_t length_;
    http_parser parser_;
    http_parser_settings settings_;
    size_t parsedn_;
    PARSE_STATE state_;
    HttpMessage* message_;
    bool nobody_ = false;
    bool tunnelbody_ = false;

private:
    HttpParserCallback headerCompletedHandle_ = nullptr;
    HttpParserCallback bodyCompletedHandle_ = nullptr;

};

} //namespace tigerso::http
#endif

