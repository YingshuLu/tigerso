#include "dns/DNSCache.h"

namespace tigerso::net {
    
int DNSNode::setAddr(const char* addr, size_t len) {
    if( addr == nullptr || len < 7 ) { return -1; }

    size_t length = len;
    if( len > IPV4_ADDRSIZE ) { length = IPV4_ADDRSIZE; }
    memcpy((void*)addr_, (void*)addr, len);
    addr_[IPV4_ADDRSIZE] = '\0';
    return 0;
}

int DNSNode::setExpiredAtTime(time_t tm) {
    expriedAt_ = tm;
    return 0;
}

int DNSNode::setMd5Key(const char* key, size_t keylen) {
    if( key == nullptr || keylen != MD5_KEYSIZE ) { return -1; }
    memcpy((void*)key_, key, MD5_KEYSIZE);
    return 0;
}
 
const char* DNSNode::getAddr() { return addr_; }

time_t DNSNode::getExpiredAtTime() { return expriedAt_; }

const unsigned char* DNSNode::getMd5Key() { return key_; }

/** DNSCache Definition  **/
DNSCache* DNSCache::pInstance_ = nullptr;

std::string DNSCache::cachefile_ = CACHE_FILE_NAME;

DNSCache* DNSCache::getInstance() {
    if(pInstance_ == nullptr) {
        pInstance_ = new DNSCache();
    }
    return pInstance_;
}

DNSCache::~DNSCache() {
    if(pInstance_ != nullptr) {
        delete pInstance_;
    }
}

DNSCache::DNSCache():
    shm_(cachefile_, sizeof(DNSCacheData)),
    mutex_(cachefile_ + ".lock") {
}

DNSCacheData* DNSCache::getShmPtr() {
    void* h = shm_.get_shm_ptr();
    if (nullptr == h) {return nullptr;}
    return (DNSCacheData*) h;
}

}


