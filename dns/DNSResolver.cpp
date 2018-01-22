#include "dns/DNSResolver.h"
#include "core/Logging.h"
/*
#include <ares.h>
#include <arpa/nameser.h>
*/

namespace tigerso {

using namespace std;

DNSResolver::DNSResolver() {
    g_DNSCachePtr = DNSCache::getInstance();
}

DNSResolver::~DNSResolver() {
}

int DNSResolver::queryDNSCache(const std::string& host, std::string& ipaddr) {
    char ip[IPV4_ADDRSIZE] = {0};
    if(!g_DNSCachePtr -> queryIP(host.c_str(), ip, IPV4_ADDRSIZE)) {
        ipaddr = ip;
        answer_name_ = ip;
        return DNS_OK;
    }
    DNS_ERR;
}

int DNSResolver::asyncQueryInit(const std::string& host, Socket& udpsock){
    query_name_ = host;
    int len = packDNSQuery(query_name_.c_str(), query_name_.size());
    //int len = aresPackDNSQuery(query_name_.c_str(), query_name_.size());
    answer_name_.clear();
    answer_ttl_ = 0;
    bzero(response_buf_,DNS_MESSAGE_LIMIT);

    udpsock.close();
    udpsock.reset();
    int ret = SocketUtil::CreateUDPConnect(primary_addr_, DNS_SERVER_PORT, true, udpsock);
    if(ret != 0) {
        INFO_LOG("UDP Socket create failed, errno: %d, %s", errno, strerror(errno));
        return DNS_ERR;
    }
    udpsock.setNIO(true);

    //std::cout << "packed query len: " << len << std::endl;
    auto obufptr = udpsock.getOutBufferPtr();
    obufptr->addData((char*)query_buf_, len);    
    return 0;
}

int DNSResolver::asyncQueryStart(EventsLoop& loop, Socket& udpsock) {
    loop.registerChannel(udpsock);
    auto cnptr = udpsock.channelptr;
    if(cnptr == nullptr) { return DNS_INPUT_ERR; }
    cnptr->setReadCallback(std::bind(&DNSResolver::recvAnswer, this, std::placeholders::_1));
    cnptr->setWriteCallback(std::bind(&DNSResolver::sendQuery, this, std::placeholders::_1));
    cnptr->setErrorCallback(std::bind(&DNSResolver::errorHandle, this, std::placeholders::_1));
    cnptr->setTimeoutCallback(std::bind(&DNSResolver::timeoutHandle, this, std::placeholders::_1));
    cnptr->enableWriteEvent();
    return DNS_OK;
}

int DNSResolver::asyncQueryStart(Socket& udpsock) {
    auto cnptr = udpsock.channelptr;
    if(cnptr == nullptr) { return DNS_INPUT_ERR; }
    cnptr->setReadCallback(std::bind(&DNSResolver::recvAnswer, this, std::placeholders::_1));
    cnptr->setWriteCallback(std::bind(&DNSResolver::sendQuery, this, std::placeholders::_1));
    cnptr->setErrorCallback(std::bind(&DNSResolver::errorHandle, this, std::placeholders::_1));
    cnptr->setTimeoutCallback(std::bind(&DNSResolver::timeoutHandle, this, std::placeholders::_1));
    cnptr->enableWriteEvent();
    return DNS_OK;
}

int DNSResolver::getAnswer(std::string& name, time_t& ttl) {
    if(answer_name_.empty()) {
        return DNS_ERR;
    }
    name = answer_name_;
    ttl = answer_ttl_;
    return DNS_OK;
}

int DNSResolver::sendQuery(Socket& udpsock) {
    int ret = udpsock.sendNIO();
    if(ret < 0) {
        INFO_LOG("UDP Socket write query failed, errno: %d, %s", errno, strerror(errno));
        return errorHandle(udpsock);
    }
    
    //std::cout << "ret: "<< ret <<", send query" << std::endl;
    auto cnptr = udpsock.channelptr;
    if(udpsock.getOutBufferPtr()->getReadableBytes() > 0) {
        return EVENT_CALLBACK_CONTINUE;
    }

    cnptr->enableReadEvent();
    cnptr->disableWriteEvent();
    return EVENT_CALLBACK_CONTINUE;
}

int DNSResolver::recvAnswer(Socket& udpsock) {
    int ret = udpsock.recvNIO();
    if(ret < 0) {
        INFO_LOG("UDP Socket read response failed, errno: %d, %s", errno, strerror(errno));
        return errorHandle(udpsock);
    }

    auto ibufptr = udpsock.getInBufferPtr();
    memcpy(response_buf_, ibufptr->getReadPtr(), ibufptr->getReadableBytes());
    ibufptr->clear();

    ret = resvDNSAnswer();
    if(ret == DNS_ID_MISMATCH) { return errorHandle(udpsock); }
    if(DNS_OK != ret) {
        return errorHandle(udpsock); 
    }

    udpsock.close();
    if(callback_ != nullptr) {
        callback_(answer_name_.c_str(), answer_ttl_);
    }
    return EVENT_CALLBACK_BREAK;
}

int DNSResolver::errorHandle(Socket& udpsock) {
    if(callback_ != nullptr) {
//        callback_(answer_name_.c_str(), answer_ttl_);
        callback_(nullptr, answer_ttl_);
    }
    udpsock.close();
    return EVENT_CALLBACK_BREAK;
}

int DNSResolver::timeoutHandle(Socket& udpsock) {
    return errorHandle(udpsock);
}

int DNSResolver::packDNSQuery(const char* query_name, size_t len) {
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
    //header.flags = htons(DNS_STANDARD_QUERY_FLAGS| DNS_HEADER_FLAGS_RD | DNS_HEADER_FLAGS_AD);
    header.flags = htons(DNS_STANDARD_QUERY_FLAGS| DNS_HEADER_FLAGS_RD);
    header.questions = htons(0x0001);
    header.answers = htons(DNS_HEAD_EMPTY);
    header.authorities = htons(DNS_HEAD_EMPTY);
    header.additions = htons(0x0001);

    //copy header to send buffer
    memcpy(cur, &header, sizeof(header));
    cur += sizeof(header);

    /*
    cout << "*********DNS Query Header**************" << endl;
    cout<< "header->id: " << ntohs(header.id) << endl;
    cout<< "header->flags: " << ntohs(header.flags) << endl;
    cout<< "header->questions: " << ntohs(header.questions) << endl;
    cout<< "header->answers: " << ntohs(header.answers) << endl;
    cout<< "header->authotities: " << ntohs(header.authorities) << endl;
    cout<< "header->additions: " << ntohs(header.additions) << endl;
    cout << "****************************************" << endl;
    */
    //Store query info
    ID_ = tid;
    query_name_ = std::string(query_name, len);

    DNSQuery query;
    query.dns_name = cur;

    unsigned char num = 0x00;
    size_t domain_len = 0;
    char domain[DNS_DOMAIN_NAME_LIMIT] = {0};
    memcpy(domain, query_name_.c_str(), query_name_.size());
    unsigned char* ptr = (unsigned char*) strtok(domain, ".");

    unsigned char* label = cur;
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

    query.dns_type = htons(DNS_TYPE_A);
    query.dns_class = htons(DNS_CLASS_INTERNET);
    memcpy(cur, &(query.dns_type), sizeof(query.dns_type));
    cur += sizeof(query.dns_type);
    memcpy(cur, &(query.dns_class), sizeof(query.dns_class));
    cur += sizeof(query.dns_class);

    //Additional OPT RR
    cur += 1; //Root
    unsigned short opt = htons(DNS_ADDITION_OPT);
    memcpy(cur, &opt, 2);
    cur += 2; //Type: OPT
    unsigned short udp_payload = htons(MAX_UDP_PAYLOAD);
    memcpy(cur, &udp_payload, 2);
    cur += 2; //UDP payload
    cur += 4; //RCODE & Flags
    cur += 2; //RDLen
    return (cur - query_buf_);
}

int DNSResolver::resvDNSAnswer() {
    unsigned char* cur = response_buf_;

    /* START RESOLVE HEADER */ 
    DNSHeader* hptr = (DNSHeader*) cur;
    /*
    cout << "*********DNS Answer Header**************" << endl;
    cout<< "query->id: " << ID_ << endl;
    cout<< "header->id: " << ntohs(hptr->id) << endl;
    cout<< "header->flags: " << ntohs(hptr->flags) << endl;
    cout<< "header->questions: " << ntohs(hptr->questions) << endl;
    cout<< "header->answers: " << ntohs(hptr->answers) << endl;
    cout<< "header->authotities: " << ntohs(hptr->authorities) << endl;
    cout<< "header->additions: " << ntohs(hptr->additions) << endl;
    cout << "****************************************" << endl;
    */
    if(strlen((char*)response_buf_) < 2) {
        return DNS_ERR;
    }

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
        //cout << "Total section : " << sections <<"\nstart resolve answers"<< endl;
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
                int ttl = (int) answer_ttl_;
                DBG_LOG("DNS Resolver get record. [Host: %s, IP: %s, ttl: %d]", query_name_.c_str(), answer_name_.c_str(), ttl);
                int code = g_DNSCachePtr->updateDNS(query_name_.c_str(), answer_name_.c_str(), ttl);
                DBG_LOG("updateDNS return : %d", code);
                return DNS_OK;
            }
            cur += resolved;
        }
    }
    /* END RR SECTIONS*/

    return DNS_ERR;
}

/*Use c-ares lib to pack query*/
int DNSResolver::aresPackDNSQuery(const char* host, size_t len) {
    /*
    int dnsclass = ns_c_in;
    int type = ns_t_a;
    int rd = 1;
    time_t seed = time(NULL);
    unsigned short tid = (unsigned short) rand_r((unsigned int*)&seed);
    int max_udp_payload = 1;

    ID_ = tid;
    unsigned char* buffer;
    int buflen;

    std::cout<< "[Ares] host: " << host << std::endl;
    int ret = ares_create_query(host, dnsclass, type, tid, rd, &buffer, &buflen, max_udp_payload);

    if( 0 != ret) { 
        std::cout << "[Ares] create query failed: " << strerror(errno) << std::endl;
        return DNS_ERR; 
    }
    std::cout << "[Ares] buffer lengtg: " << buflen << std::endl;
    memcpy(query_buf_, buffer, buflen);
    ares_free_string(buffer);
    return buflen;
    */
    return DNS_OK;
}

int DNSResolver::aresResvDNSAnswer() {
    return DNS_OK;
}

int DNSResolver::resolveRR(unsigned char* buf, size_t buf_len, unsigned char* curp, unsigned char* name, size_t name_len, unsigned short* type, unsigned short* rclass, time_t* ttl, unsigned char* rdata) {
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

int DNSResolver::resolveRRName(unsigned char* buf, size_t buf_len, unsigned char* curp, unsigned char* pname, size_t name_len) {
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
        if(0 > resolveRRName(buf, buf_len, buf + size_t(offset), name, name_len)) {
            return DNS_RESOLVE_ERR;
        }
        cur += 2;
    }

    return (cur - curp);
}
    
std::string DNSResolver::primary_addr_ = DNS_SERVER_ADDR;
std::string DNSResolver::second_addr_ = "";

} //namespace tigerso::dns
