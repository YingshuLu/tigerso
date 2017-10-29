
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
/*
#include "net/Socket.h"
#include "core/BaseClass.h"
extern DNSCache* DNSCachePtr;
*/

#define DNS_STANDARD_QUERY_FLAGS       0x0100
#define DNS_STANDARD_ANSWER_FLAGS      0x8180

/*DNS TYPE MACROS*/
#define DNS_TYPE_A                      0x0001
#define DNS_TYPE_NS                     0x0002
#define DNS_TYPE_CNAME                  0x0005
#define DNS_TYPE_SOA                    0x0006
#define DNS_TYPE_WKS                    0x000b
#define DNS_TYPE_PTR                    0x000c
#define DNS_TYPE_HINFO                  0x000d
#define DNS_TYPE_MX                     0x000e
#define DNS_TYPE_AAAA                   0x001c
#define DNS_TYPE_AXFR                   0x00ec
#define DNS_TYPE_ANY                    0x00ee


#define DNS_CLASS_INTERNET              0x0001

#define DNS_OK         0
#define DNS_ERR       -1
#define IPV4_ADDRSIZE 16

#define DNS_SERVER_ADDR "8.8.8.8"
#define DNS_SERVER_PORT 53

using namespace std;
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



class DNSResolver {

public:
    DNSResolver() {}

    std::string queryDNS(std::string& name) {
        query_name_ = name;
        if(sendQuery() < 0) {
            cout << "send err" <<endl;
            return string("");
        }
        if(recvAnswer() < 0) {
            cout << "recv error" <<endl;
        }
        return answer_name_;
    }

    int getAnswer(string& name, time_t& ttl) {
        if(answer_name_.empty()) {
            return -1;
        }

        name = answer_name_;
        ttl = answer_ttl_;
        return 0;
    }

private:
    int packDNSQuery(const char* query_name, size_t len) {
        if(query_name == NULL) {
            return DNS_ERR;
        }

        bzero(query_buf_, sizeof(query_buf_));
        unsigned char* cur = query_buf_;

        DNSHeader header;
        
        //debug
        unsigned short tid = (unsigned short) rand();

        cout << "query->id: " << tid <<endl;
        header.id = htons(tid);
        header.flags = htons(DNS_STANDARD_QUERY_FLAGS);
        header.questions = htons(0x0001);
        header.answers = htons(0x0000);
        header.authorities = htons(0x0000);
        header.additions = htons(0x0000);


        //copy header to send buffer
        memcpy(cur, &header, sizeof(header));
        cur += sizeof(header);

        //Store query info
        ID_ = header.id;
        query_name_ = std::string(query_name, len);

        DNSQuery query;
        query.dns_name = cur;
        
        cout << "header packed, size: " << cur - query_buf_ <<endl;

        unsigned char num = 0x00;
        size_t domain_len = 0;
        char* domain = (char*)malloc(len);
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
        free(domain);

        cout << "domain packed, size: " << cur - query_buf_ <<endl;
        query.dns_type = htons(DNS_TYPE_A);
        query.dns_class = htons(DNS_CLASS_INTERNET);
        memcpy(cur, &(query.dns_type), sizeof(query.dns_type));
        cur += sizeof(query.dns_type);
        memcpy(cur, &(query.dns_class), sizeof(query.dns_class));
        cur += sizeof(query.dns_class);

        return (cur - query_buf_);
        
    }

    int resvDNSAnswer() {
        unsigned char* cur = answer_buf_;

        /* START RESOLVE HEADER */ 
        DNSHeader* hptr = (DNSHeader*) cur;
        if (ID_ != ntohs(hptr->id) 
            || DNS_STANDARD_ANSWER_FLAGS != ntohs(hptr->flags)
            || 0x0001 > ntohs(hptr->answers) 
           ) {
            cout << "answer header info not match" <<endl;
            //return DNS_ERR;
        }

        cout<< "header->id: " << ntohs(hptr->id) << endl;
        cout<< "header->flags: " << ntohs(hptr->flags) << endl;
        cout<< "header->answers " << ntohs(hptr->answers) << endl;

        cur += sizeof(DNSHeader);
        /* END  HEADER */ 

        /* START RESOLVE QUERY */
        unsigned char num = *cur;
        size_t domain_len = 0; 
        while(0x00 != num) {
            cur ++;
            domain_len = (size_t) num;
            cur += domain_len;
            num = *cur;
        }
        cur++;

        //skip query type check
        cur += 2;
        //skip query class check
        cur += 2;
        /* END QUERY */

        /* START RESOLVE ANSWER */

        for(int i = 0; i < int(ntohs(hptr->answers)); i++) {
            //skip query name pointor
            cur += 2;

            //check type, skip not IPV4 answers
            unsigned short type = ntohs(*((unsigned short*) cur));
            cout << "type: " << type <<endl;
            if(DNS_TYPE_A != type) {
                cout << "answer not IPV4" <<endl;
                //Skip class, ttl
                cur += 8;

                //get data len
                unsigned short data_len = ntohs(*((unsigned short*) cur));
                cout << "data len:" << data_len <<endl;
                cur += 2;
                //skip data
                cur += size_t(data_len);
                continue;
            }

            // resolve IPV4 Answer
            cur += 2;

            //skip class check
            cur += 2;

            //get ttl
            answer_ttl_ = ntohl(*((time_t*) cur));
            cur += 4;

            //skip data len
            cur += 2;

            //get IP
            char ip[IPV4_ADDRSIZE] = {0};

            strncpy(ip, inet_ntoa(*((struct in_addr*)cur)), IPV4_ADDRSIZE);

            answer_name_ = ip;

            break;    
        }

        return DNS_OK;
    }


    int sendQuery() {

        int len = packDNSQuery(query_name_.c_str(), query_name_.size());
        if(len == DNS_ERR) {
            cout << "Pack DNS query error" <<endl;
            return DNS_ERR;
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
        cout << "send " << sendn << " bytes" <<endl;
        return sendn;
    }

    int recvAnswer() {
        if(sockfd_ < 0) {
            return DNS_ERR;
        }

        socklen_t len = socklen_t(sizeof(server_addr_));

        int recvn = recvfrom(sockfd_, (void*) answer_buf_, sizeof(answer_buf_), 0, (sockaddr*) &server_addr_, &len);
        if(recvn == -1) {
            cout << "recvfrom return -1" <<endl;
            return DNS_ERR;
        }

        cout << "recv " << recvn << " bytes" <<endl;
        return resvDNSAnswer();    
    }
    
private:
    unsigned char query_buf_ [1024] = {0};
    unsigned char answer_buf_[2048] = {0};

    unsigned short ID_ = 0x00;
    socket_t sockfd_ = -1;

    sockaddr_in server_addr_;
private:
    std::string query_name_;
    std::string answer_name_;
    time_t answer_ttl_;
    

};
