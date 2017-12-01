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
#include "http/HttpMessage.h"

namespace tigerso::http {
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

class HttpParser: public core::nocopyable {
public:
    HttpParser(): buffer_(nullptr), length_(0), parsedn_(0), state_(PARSE_UNINIT), message_(nullptr) {}
    int parse(const char*, size_t, HttpMessage&);
    int parse(const std::string&, HttpMessage&);
    inline PARSE_STATE getParseState() { return state_; }
    bool skipBody() { return skipbody_; }
    bool headerCompleted() {  return (state_ >= PARSE_HEADER_COMPLETE? true : false); }
    bool needMoreData() { return state_ != PARSE_COMPLETE; }
    const char* getStrErr() { return http_errno_description(HTTP_PARSER_ERRNO(&parser_)); }
    size_t getLastParsedSize() { return parsedn_; }
    bool isBigFile() { return bigfile_; }
    void reset();

    //only for http parser callback
public:
    bool labelBigFile() { bigfile_ = true; }
    void setParseState(PARSE_STATE state) { state_ = state; }
    void setSkipBody(bool skip) { skipbody_ = skip; }
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

private:
    const char* buffer_;
    size_t length_;
    http_parser parser_;
    http_parser_settings settings_;
    size_t parsedn_;
    PARSE_STATE state_;
    bool skipbody_ = false;
    HttpMessage* message_;

private:
    bool bigfile_ = false;
};

} //namespace tigerso::http
#endif

