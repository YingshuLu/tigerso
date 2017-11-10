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
#include "net/Socket.h"
#include "core/BaseClass.h"
#include "dns/DNSCache.h"

namespace tigerso::dns {

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
#define DNS_SERVER_PORT 53
#define DNS_MESSAGE_LIMIT 512          
#define DNS_DOMAIN_NAME_LIMIT 255
#define DNS_RRNAME_POINTERSIZE 2

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

static DNSCache* g_DNSCachePtr = DNSCache::getInstance(); 

class DNSResolver {

public:
    DNSResolver() {}

    std::string queryDNS(std::string& name) {
        query_name_ = name;

        char buf[IPV4_ADDRSIZE] = {0};
        if(!g_DNSCachePtr -> queryIP(query_name_.c_str(), buf, IPV4_ADDRSIZE)) {
            answer_name_ = buf;
            return answer_name_;
        }

        if(sendQuery() < 0) {
            //cout << "send err" <<endl;
            return std::string("");
        }
        if(recvAnswer() < 0) {
            //cout << "recv error" <<endl;
        }
        return answer_name_;
    }

    int getAnswer(std::string& name, time_t& ttl) {
        if(answer_name_.empty()) {
            return -1;
        }

        name = answer_name_;
        ttl = answer_ttl_;
        return 0;
    }

    static void setPrimaryAddr(const std::string& paddr) { primary_addr_ = paddr; }
    static void setSecondAddr(const std::string& saddr) { second_addr_ = saddr; }

private:
    DNSResolver(const DNSResolver&){}
    DNSResolver& operator=(const DNSResolver&){}

private:
    int packDNSQuery(const char* query_name, size_t len) {
        if(query_name == NULL) {
            return DNS_INPUT_ERR;
        }

        bzero(query_buf_, sizeof(query_buf_));
        unsigned char* cur = query_buf_;

        DNSHeader header;
        
        // Generate transaction ID
        time_t seed = time(NULL);
        unsigned short tid = (unsigned short) rand_r((unsigned int*)&seed);

        //cout << "query->id: " << tid <<endl;
        header.id = htons(tid);
        header.flags = htons(FLAGS_CONBIME(DNS_STANDARD_QUERY_FLAGS, DNS_HEADER_FLAGS_RA));
        header.questions = htons(0x0001);
        header.answers = htons(DNS_HEAD_EMPTY);
        header.authorities = htons(DNS_HEAD_EMPTY);
        header.additions = htons(DNS_HEAD_EMPTY);

        //copy header to send buffer
        memcpy(cur, &header, sizeof(header));
        cur += sizeof(header);

        //Store query info
        ID_ = tid;
        query_name_ = std::string(query_name, len);

        DNSQuery query;
        query.dns_name = cur;
        
        unsigned char num = 0x00;
        size_t domain_len = 0;
        char domain[DNS_DOMAIN_NAME_LIMIT] = {0};
        memcpy(domain, query_name, strlen(query_name));
        unsigned char* ptr = (unsigned char*) strtok(domain, ".");
       
        //copy query to send buffer
        while(NULL != ptr) {
            domain_len = strlen((const char*)ptr);
            num = (unsigned char) domain_len;
            memcpy(cur, &num, sizeof(num));
            cur++;

            memcpy(cur, ptr, domain_len);
            cur += domain_len;
            ptr = (unsigned char*) strtok(NULL, ".");
        }
        cur++;

        ////cout << "domain packed, size: " << cur - query_buf_ <<endl;
        query.dns_type = htons(DNS_TYPE_A);
        query.dns_class = htons(DNS_CLASS_INTERNET);
        memcpy(cur, &(query.dns_type), sizeof(query.dns_type));
        cur += sizeof(query.dns_type);
        memcpy(cur, &(query.dns_class), sizeof(query.dns_class));
        cur += sizeof(query.dns_class);

        return (cur - query_buf_);
    }

    int resvDNSAnswer() {
        unsigned char* cur = response_buf_;

        /* START RESOLVE HEADER */ 
        DNSHeader* hptr = (DNSHeader*) cur;
        //cout<< "saved id: " << ID_ << endl;
        //cout<< "header->id: " << ntohs(hptr->id) << endl;
        //cout<< "header->flags: " << ntohs(hptr->flags) << endl;
        //cout<< "header->questions " << ntohs(hptr->questions) << endl;
        //cout<< "header->answers " << ntohs(hptr->answers) << endl;
        //cout<< "header->authotities: " << ntohs(hptr->authorities) << endl;
        //cout<< "header->additions: " << ntohs(hptr->additions) << endl;

        if (ID_ != ntohs(hptr->id)) {
            //cout << "ID Mismatched!" <<endl;
            return DNS_ID_MISMATCH;
        }
        if(0 != BITS_COMPARE(DNS_STANDARD_ANSWER_FLAGS,ntohs(hptr->flags) || 0x0001 > ntohs(hptr->answers))) {
            //cout << "answer header info not match" <<endl;
            //return DNS_ERR;
        }

        cur += sizeof(DNSHeader);
        /* END  HEADER */ 

        /* START RESOLVE QUERY SECTION*/
        unsigned char RRName[DNS_DOMAIN_NAME_LIMIT] = {0};
        int resolved = resolveRRName(response_buf_, sizeof(response_buf_), cur, RRName, sizeof(RRName));
        if(0 > resolved) {return resolved;}
        //cout << "resolve query name: " << RRName <<endl;
        cur += resolved;
        //skip query type check
        cur += 2;
        //skip query class check
        cur += 2;
        /* END QUERY SECTION */

        /* START RESOLVE RR SECTIONS */
        int RRNums[3] = {ntohs(hptr->answers), ntohs(hptr->authorities), ntohs(hptr->additions)};
        for (int n = 0; n < 3; n++) {
            int sections =  RRNums[n];

            //Empty Answers
            if(0 == sections && 0 == n) {
                return DNS_ERR;        
            }
            //cout << "Total section : " << sections <<"\n start resolve answers"<< endl;
            for(int i = 0; i < sections; i++) {
                unsigned char rdomain[DNS_DOMAIN_NAME_LIMIT] = {0};
                unsigned char rdata[DNS_DOMAIN_NAME_LIMIT] = {0};
                unsigned short rtype = 0;
                unsigned short rclass = 0;
                time_t rttl = 0;

                int resolved = resolveRR(response_buf_, sizeof(response_buf_), cur, rdomain, DNS_DOMAIN_NAME_LIMIT, &rtype, &rclass, &rttl, rdata);
                if(0 > resolved) {
                    return resolved;
                }

                // Get First IP Address from Answer(s)
                if( 0 == n && 0 == BITS_COMPARE(rtype, DNS_TYPE_A)) { 
                    answer_name_ = std::string((char*)rdata); 
                    answer_ttl_ = rttl;
                    return DNS_OK;
                }
                cur += resolved;
            }
        }
        /* END RR SECTIONS*/

        return DNS_ERR;
    }


    int sendQuery() {
        int len = packDNSQuery(query_name_.c_str(), query_name_.size());
        if(len < 0) {
            //cout << "Pack DNS query error" <<endl;
            return len;
        }

        sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if(sockfd_ < 0) {
            return DNS_ERR;
        }

        bzero(&server_addr_, sizeof(server_addr_));
        server_addr_.sin_family = AF_INET;
        inet_aton(DNS_SERVER_ADDR, &server_addr_.sin_addr);
        server_addr_.sin_port = htons(DNS_SERVER_PORT);

        size_t addr_len = sizeof(server_addr_);

        int sendn = sendto(sockfd_, query_buf_, len, 0, (sockaddr*) &server_addr_, addr_len);
        //cout << "send " << sendn << " bytes" <<endl;
        return sendn;
    }

    int recvAnswer() {
        if(sockfd_ < 0) {
            //cout << "Invalid sockfd" <<endl;
            return DNS_ERR;
        }

        socklen_t len = socklen_t(sizeof(server_addr_));

        int recvn = recvfrom(sockfd_, (void*) response_buf_, sizeof(response_buf_), 0, (sockaddr*) &server_addr_, &len);
        if(recvn == -1) {
            //cout << "recvfrom return -1" <<endl;
            return DNS_ERR;
        }

        //cout << "recv " << recvn << " bytes" <<endl;
        return resvDNSAnswer();    
    }

private:
    int resolveRR(unsigned char* buf, size_t buf_len, unsigned char* curp, unsigned char* name, size_t name_len, unsigned short* type, unsigned short* rclass, time_t* ttl, unsigned char* rdata) {
        if(NULL == buf 
           || NULL == curp
           || NULL == name
           || NULL == type
           || NULL == rclass
           || NULL == ttl 
           || NULL == rdata) {
            return DNS_INPUT_ERR;
        }

        unsigned char* cur = curp;

        //Get Name
        int resolved = resolveRRName(buf, buf_len, cur, name, name_len);
        if(0 > resolved) {
            return DNS_RESOLVE_ERR;
        }
        //cout << "RR >>  name: " << name <<endl;
        cur += resolved;

        //Get Type
        *type = ntohs(*((unsigned short*) cur));
        cur += 2;
        //cout << "RR >> Type: " << *type <<endl;

        //Get Class
        *rclass = ntohs(*((unsigned short*) cur));
        cur += 2;
        //cout << "RR >> class: " << *rclass <<endl;

        //Get TTL
        *ttl = ntohl(*((int*)cur));
        cur += 4;
        //cout << "RR >> TTL: " << *ttl <<endl;

        //Get RDLENGTH
        unsigned short num = ntohs(*((unsigned short*) cur));      
        cur += 2;
        //cout << "RR >> RData length: " << size_t(num) <<endl;
        
        //Get RData
        //Data is a pointer
        if(DNS_RRNAME_POINTERSIZE == num && startWithPointer(cur)) {
            int res = resolveRRName(response_buf_, sizeof(response_buf_), cur, rdata, 0); 
            if(0 > res) { return res; }
        }
        else { memcpy(rdata, cur, size_t(num)); }

        if(BITS_COMPARE(*type, DNS_TYPE_A) == 0) {
            char ip[IPV4_ADDRSIZE] = {0};
            strncpy(ip, inet_ntoa(*((struct in_addr*)rdata)), IPV4_ADDRSIZE);
            memcpy(rdata, ip, sizeof(ip));
        }
        //cout << ">> response rdata: " << rdata <<endl;

        cur += size_t(num);
        return (cur - curp);
    }

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
    int resolveRRName(unsigned char* buf, size_t buf_len, unsigned char* curp, unsigned char* pname, size_t name_len) {
        if(NULL == buf || NULL == curp || NULL == pname) {
            return DNS_INPUT_ERR;
        }

        unsigned char* cur = curp;
        unsigned char* name = pname;
        unsigned char num = *cur;
        while(0x00 != num && startWithLabel(cur)) {
           cur ++;
           memcpy(name, cur, size_t(num));
           name += size_t(num);
           cur += size_t(num);
           memcpy(name, ".", 1);
           name ++;
           num = *cur;
        }

        if(0x00 == num) {cur++;}
        else if(startWithPointer(cur)) {
            unsigned char high = *cur & ~DNS_RRNAME_OFFSETFLAG;
            unsigned char low = *(cur+1);
            unsigned short offset = high;
            offset = offset << 8;
            offset = high + low;
            //cout << "RRname Offset : " << offset <<endl;
            if(0 > resolveRRName(buf, buf_len, buf + size_t(offset), name, name_len)) {
                 return DNS_RESOLVE_ERR;
            }
            cur += 2;
        }

        return (cur - curp);
    }
    
private:
    unsigned char query_buf_ [DNS_MESSAGE_LIMIT] = {0};
    unsigned char response_buf_[DNS_MESSAGE_LIMIT] = {0};

    unsigned short ID_ = 0x00;
    socket_t sockfd_ = -1;
    sockaddr_in server_addr_;

private:
    std::string query_name_;
    std::string answer_name_;
    time_t answer_ttl_ = 0;

private:
    std::string assigned_addr_ = "";
    static std::string primary_addr_;
    static std::string second_addr_;
};

std::string DNSResolver::primary_addr_ = DNS_SERVER_ADDR;
std::string DNSResolver::second_addr_ = "";

} //namespace tigerso::dns
#endif
