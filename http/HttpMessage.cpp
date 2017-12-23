#include "http/HttpMessage.h"

namespace tigerso {

typedef std::map<int, std::string>::value_type  status_pair_t;
static const status_pair_t response_status_array [] = {
    status_pair_t(100, "Continue"),
    status_pair_t(101, "Switching Protocols"),
    status_pair_t(200, "OK"),
    status_pair_t(201, "Created"),
    status_pair_t(202, "Accepted"),
    status_pair_t(203, "Non-Authoritative Information"),
    status_pair_t(204, "No Content"),
    status_pair_t(205, "Reset Content"),
    status_pair_t(206, "Partial Content"),
    status_pair_t(207, "Multi-Status"),
    status_pair_t(300, "Multiple Choices"),
    status_pair_t(301, "Moved Permanently"),
    status_pair_t(302, "Found"),
    status_pair_t(303, "See Other"),
    status_pair_t(304, "Not Modified"),
    status_pair_t(305, "Use Proxy"),
    status_pair_t(306, "Switch Proxy"),
    status_pair_t(307, "Temporary Redirect"),
    status_pair_t(400, "Bad Request"),
    status_pair_t(401, "Unauthorized"),
    status_pair_t(402, "Payment Required"),
    status_pair_t(403, "Forbidden"),
    status_pair_t(404, "Not Found "),
    status_pair_t(405, "Method Not Allowed"),
    status_pair_t(406, "Not Acceptable"),
    status_pair_t(407, "Proxy Authentication Required"),
    status_pair_t(408, "Request Timeout"),
    status_pair_t(409, "Conflict"),
    status_pair_t(410, "Gone"),
    status_pair_t(411, "Length Required"),
    status_pair_t(412, "Precondition Failed"),
    status_pair_t(413, "Request Entity Too Large"),
    status_pair_t(414, "Request-URI Too Long"),
    status_pair_t(415, "Unsupported Media Type"),
    status_pair_t(416, "Requested Range Not Satisfiable"),
    status_pair_t(417, "Expectation Failed"),
    status_pair_t(422, "Unprocessable Entity"),
    status_pair_t(423, "Locked"),
    status_pair_t(424, "Failed Dependency"),
    status_pair_t(425, "Unorder Collection"),
    status_pair_t(426, "Upgrade Required"),
    status_pair_t(428, "Precondition Required"),
    status_pair_t(429, "Too Many Requests"),
    status_pair_t(431, "Request Header Fields Too Large"),
    status_pair_t(449, "Retry With"),
    status_pair_t(451, "Unavailable For Legal Reasons"),
    status_pair_t(500, "Internal Server Error"),
    status_pair_t(501, "Not Implemented"),
    status_pair_t(502, "Bad Gateway"),
    status_pair_t(503, "Service Unavailable"),
    status_pair_t(504, "Gateway Timeout"),
    status_pair_t(505, "HTTP Version Not Supported"),
    status_pair_t(506, "Variant Also Negotiates"),
    status_pair_t(507, "Insufficient Storage"),
    status_pair_t(509, "Bandwidth Limit Exceeded"),
    status_pair_t(510, "Not Extend"),
    status_pair_t(511, "Network Authentication Required"),
    status_pair_t(600, "Unparseable Response Headers")
};

const std::map<int, std::string> HttpHelper::RESPONSE_STATUS_MAP(response_status_array, response_status_array + (sizeof(response_status_array)/sizeof(response_status_array[0])));


static const std::string defaultResponse403Body = "<html><head><title>TIGERSO NOTIFICATION</title></head><body><p>You Are Forbidden To Access This Website!</p></body></html>";
static const std::string defaultResponse503Body = "<html><head><title>TIGERSO NOTIFICATION</title></head><body><p> Service Unavailable!</p></body></html>";
static const std::string defaultResponse504Body = "<html><head><title>TIGERSO NOTIFICATION</title></head><body><p>Gateway Timeout! Please Inspect Your Gateway.</p></body></html>";

static const std::string defaultResponseDNSErrorBody = "<html><head><title>TIGERSO NOTIFICATION</title></head><body><p>Gateway Timeout! Please Inspect Your Gateway.</p></body></html>";

int HttpHelper::prepare200Response(HttpResponse& response) {
    response.setStatuscode(200);
    response.setDesc(getResponseStatusDesc(200));
    response.setVersion("HTTP/1.1");

    response.setValueByHeader("Connection", "keep-alive");
    response.markTrade();
    return 0;
}

int HttpHelper::prepare400Response(HttpResponse& response) {
    return prepare403Response(response);
}

int HttpHelper::prepare403Response(HttpResponse& response) {
    response.setStatuscode(403);
    response.setDesc(getResponseStatusDesc(403));
    response.setVersion("HTTP/1.1");

    response.setValueByHeader("Connection", "close");
    response.markTrade();
    char content_length[60] = {0};
    sprintf(content_length, "%u", defaultResponse403Body.size());
    response.setValueByHeader("Content-length", content_length);
    response.setBody(defaultResponse403Body); 
    return 0;
}

int HttpHelper::prepare503Response(HttpResponse& response) {
    response.setStatuscode(503);
    response.setDesc(getResponseStatusDesc(503));
    response.setVersion("HTTP/1.1");

    response.setValueByHeader("Connection", "close");
    response.markTrade();
    char content_length[60] = {0};
    sprintf(content_length, "%u", defaultResponse503Body.size());
    response.setValueByHeader("Content-length", content_length);
    response.setBody(defaultResponse503Body);
    return 0;
}

int HttpHelper::prepare504Response(HttpResponse& response) {
    response.setStatuscode(504);
    response.setDesc(getResponseStatusDesc(504));
    response.setVersion("HTTP/1.1");

    response.setValueByHeader("Connection", "close");
    response.markTrade();
    char content_length[60] = {0};
    sprintf(content_length, "%u", defaultResponse504Body.size());
    response.setValueByHeader("Content-length", content_length);
    response.setBody(defaultResponse504Body);
    return 0;
}

int HttpHelper::prepareDNSErrorResponse(HttpResponse& response) {
    response.setStatuscode(503);
    response.setDesc(getResponseStatusDesc(503));
    response.setVersion("HTTP/1.1");

    response.setValueByHeader("Connection", "close");
    response.markTrade();
    char content_length[60] = {0};
    sprintf(content_length, "%u", defaultResponseDNSErrorBody.size());
    response.setValueByHeader("Content-length", content_length);
    response.setBody(defaultResponseDNSErrorBody);
    return 0;
}

bool HttpHelper::isVaildResponseStatusCode(const int code) {
    return (RESPONSE_STATUS_MAP.find(code) != RESPONSE_STATUS_MAP.end());
}

std::string HttpHelper::getResponseStatusDesc(const int code) {
    auto iter = RESPONSE_STATUS_MAP.find(code);
    if( iter == RESPONSE_STATUS_MAP.end() ) {
        return std::string("");
    }
    return iter->second;
}


const std::string HttpMessage::METHOD = "METHOD";
const std::string HttpMessage::URL = "URL";
const std::string HttpMessage::VERSION = "VERSION";
const std::string HttpMessage::STATUSCODE = "STATUSCODE";
const std::string HttpMessage::DESC = "DESC";

const std::string HttpResponse::OK = "HTTP/1.1 200 OK\r\nserver: tigerso/" + core::VERSION + "\r\n\r\n";
const std::string HttpResponse::BAD_REQUEST = "HTTP/1.1 400 Bad Request\r\nserver: tigerso/" + core::VERSION + "\r\n\r\n";
const std::string HttpResponse::NOT_FOUND = "HTTP/1.1 404 Not Found\r\nserver: tigerso/" + core::VERSION + "\r\n\r\n";
const std::string HttpResponse::FORBIDDEN = "HTTP/1.1 403 Forbidden\r\nserver: tigerso/" + core::VERSION + "\r\n\r\n";

//Http rule inspection
int HttpInSpection::Inspect(const std::string& header,  const std::weak_ptr<HttpMessage>& wptr) {
    httpInspectCallback fptr = nullptr;
    std::shared_ptr<HttpMessage> sptr = wptr.lock();
    if( !sptr ) { return HTTP_INSPECTION_CONTINUE; }

    auto &callbacks_ = sptr->getRole() == HTTP_ROLE_REQUEST? requestCallbacks_ : responseCallbacks_;
    for ( auto iter = callbacks_.begin(); iter != callbacks_.end(); iter++ ) {
        if ( strcasecmp(header.c_str(), iter->first.c_str()) == 0 ) {
            fptr = iter->second; 
            break;
        }
    }

    if (fptr == nullptr) { return HTTP_INSPECTION_CONTINUE; }
    return fptr(wptr);
}

void HttpInSpection::Register(const std::string& header, httpInspectCallback callback, http_role_t role) { 
    auto &callbacks_ = role == HTTP_ROLE_REQUEST? requestCallbacks_ : responseCallbacks_;
    for ( auto iter = callbacks_.begin(); iter != callbacks_.end(); iter++ ) {
        if ( strcasecmp(header.c_str(), iter->first.c_str()) == 0 ) {
            callbacks_[iter->first] = callback; 
            return;
        }
    }
    callbacks_[header] = callback;
}

void HttpInSpection::Unregister(const std::string& header, http_role_t role) {
    auto &callbacks_ = role == HTTP_ROLE_REQUEST? requestCallbacks_ : responseCallbacks_;
    for ( auto iter = callbacks_.begin(); iter != callbacks_.end(); iter++ ) {
        if ( strcasecmp(header.c_str(), iter->first.c_str()) == 0 ) {
            callbacks_.erase(iter);
            return;
        }
    }
    return;
}
} //namespace mcutil
