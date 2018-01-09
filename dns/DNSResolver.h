#ifndef TS_DNS_DNSRESOLVER_H_
#define TS_DNS_DNSRESOLVER_H_

#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <string>
#include <iostream>
#include <functional>
#include "net/Socket.h"
#include "net/SocketUtil.h"
#include "net/Channel.h"
#include "net/EventsLoop.h"
#include "core/BaseClass.h"
#include "dns/DNSCache.h"

namespace tigerso {


/*
 * Marco definitions refer to DNS RFC 1035:
 * https://tools.ietf.org/html/rfc1035  
*/

#define DNS_HEAD_EMPTY                 0x0000

#define DNS_STANDARD_QUERY_FLAGS       0x0100
#define DNS_STANDARD_ANSWER_FLAGS      0x8180

/*DNS Header Flags Macros*/
#define DNS_HEADER_FLAGS_QR_QUERY            0x0000
#define DNS_HEADER_FLAGS_QR_RESPONSE         0x8000
#define DNS_HEADER_FLAGS_OPCODE_QUERY        0x0000
#define DNS_HEADER_FLAGS_OPCODE_IQUERY       0x0800
#define DNS_HEADER_FLAGS_OPCODE_STATUS       0x1000
#define DNS_HEADER_FLAGS_AA                  0x0400
#define DNS_HEADER_FLAGS_TC                  0x0200
#define DNS_HEADER_FLAGS_RD                  0x0100
#define DNS_HEADER_FLAGS_RA                  0x0080
#define DNS_HEADER_FLAGS_AD                  0x0020
#define DNS_HEADER_FLAGS_RCODE_NONE_ERR      0x0000
#define DNS_HEADER_FLAGS_RCODE_FRMT_ERR      0x0001
#define DNS_HEADER_FLAGS_RCODE_SEVR_ERR      0x0002
#define DNS_HEADER_FLAGS_RCODE_NAME_ERR      0x0003
#define DNS_HEADER_FLAGS_RCODE_SPRT_ERR      0x0004
#define DNS_HEADER_FLAGS_RCODE_REFUSED       0x0005

#define FLAGS_CONBIME(flagset,flag) (flagset|flag)
#define FLAGS_CONTAIN(flagset,flag) (flagset&flag)
#define BITS_COMPARE(a,b) (a^b)

/*DNS TYPE MACROS*/
#define DNS_TYPE_A                      0x0001  /*IPV4*/
#define DNS_TYPE_NS                     0x0002
#define DNS_TYPE_CNAME                  0x0005
#define DNS_TYPE_SOA                    0x0006
#define DNS_TYPE_WKS                    0x000b
#define DNS_TYPE_PTR                    0x000c
#define DNS_TYPE_HINFO                  0x000d
#define DNS_TYPE_MX                     0x000e
#define DNS_TYPE_AAAA                   0x001c  /*IPV6*/
#define DNS_TYPE_AXFR                   0x00ec
#define DNS_TYPE_ANY                    0x00ee

#define DNS_CLASS_INTERNET              0x0001
#define DNS_RRNAME_OFFSETFLAG           0xc0

#define DNS_OK             0
#define DNS_ERR           -1
#define DNS_INPUT_ERR     -2
#define DNS_RESOLVE_ERR   -3
#define DNS_ID_MISMATCH   -4
#define DNS_NOANSWERS     -5

#define IPV4_ADDRSIZE 16
#define DNS_SERVER_ADDR "8.8.8.8"
#define DNS_SERVER_PORT "53"
#define DNS_MESSAGE_LIMIT 512          
#define DNS_DOMAIN_NAME_LIMIT 255
#define DNS_RRNAME_POINTERSIZE 2

#define DNS_NEED_MORE_DATA 40
#define DNS_SEND_ERR DNS_ERR
#define DNS_RECV_ERR DNS_ERR
#define DNS_SOCKET_IO_COMPLETE DNS_OK

/*EDNS, refer to rfc2671*/
#define MAX_UDP_PAYLOAD 1280
#define DNS_ADDITION_OPT 0x0029

typedef int socket_t;

struct DNSHeader {
    unsigned short id;
    unsigned short flags;
    unsigned short questions;
    unsigned short answers;
    unsigned short authorities;
    unsigned short additions;
};

struct DNSQuery {
    unsigned char* dns_name;
    unsigned short dns_type;
    unsigned short dns_class;
};

struct DNSAnswer {
    unsigned char* dhs_name;
    unsigned short dns_type;
    unsigned short dns_class;
    unsigned short dns_ttl;
    char* dns_data;
};

typedef struct DNSAnswer DNSAuthority;
typedef struct DNSAnswer DNSAddition;
typedef std::function<int(const char*, time_t)> DNS_CALLBACK;

class DNSResolver {

public:
    DNSResolver();

    int queryDNSCache(const std::string& host, std::string& ipaddr);
    int asyncQueryInit(const std::string& host, Socket& udpsock);
    int setCallback(DNS_CALLBACK cb) {callback_ = cb;}

    int asyncQueryStart(EventsLoop& loop, Socket& udpsock);

    int getAnswer(std::string& name, time_t& ttl);
    static void setPrimaryAddr(const std::string& paddr) { primary_addr_ = paddr; }
    static void setSecondAddr(const std::string& saddr) { second_addr_ = saddr; }

    int sendQuery(Socket& udpsock);
    int recvAnswer(Socket& udpsock);
    int errorHandle(Socket& udpsock);
    int timeoutHandle(Socket& udpsock);

    ~DNSResolver();
private:
    /*nocopyable*/
    DNSResolver(const DNSResolver&){}
    DNSResolver& operator=(const DNSResolver&){}

private:
    int packDNSQuery(const char* query_name, size_t len);
    int resvDNSAnswer();

private:
    int aresPackDNSQuery(const char* query_name, size_t len);
    int aresResvDNSAnswer();

private:
    int resolveRR(unsigned char* buf, size_t buf_len, unsigned char* curp, unsigned char* name, size_t name_len, unsigned short* type, unsigned short* rclass, time_t* ttl, unsigned char* rdata);
   
    inline bool startWithPointer(unsigned char* cur) {
        return !startWithLabel(cur);
    }

    inline bool startWithLabel(unsigned char* cur) {
        unsigned char flag = DNS_RRNAME_OFFSETFLAG;
        unsigned char si  = *cur & ~flag;
        if (0x00 == si) {
            return false;
        }
        return true;
    }

    /* Get Name from RR section
     * buf: UDP messages
     * curp: current pointer to buffer position, will be changed after called this function
     * name: RR Name pointer, need declare and alloc memory before call this function
     * 
     * return
     * Success: resolved size
     * Failure: DNS_ERR, DNS_INPUT_ERR
    */
    int resolveRRName(unsigned char* buf, size_t buf_len, unsigned char* curp, unsigned char* pname, size_t name_len);
   
private:
    unsigned char query_buf_ [DNS_MESSAGE_LIMIT] = {0};
    unsigned char response_buf_[DNS_MESSAGE_LIMIT] = {0};

    unsigned short ID_ = 0x00;
    socket_t sockfd_ = -1;
    sockaddr_in server_addr_;

private:
    DNS_CALLBACK callback_ = nullptr;

private:
    std::string query_name_;
    std::string answer_name_;
    time_t answer_ttl_ = 0;

private:
    std::string assigned_addr_ = "";
    static std::string primary_addr_;
    static std::string second_addr_;
    static DNSCache* g_DNSCachePtr; 
};

} //namespace tigerso::dns
#endif
