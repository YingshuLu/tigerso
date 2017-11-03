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

int DNSCache::queryIP(const char* host, char* ipaddr, size_t len) {
    if(nullptr == host || nullptr == ipaddr) {
        return -1;
    }

    unsigned char md5[MD5_KEYSIZE] = {0};
    hashkey_t key = getHashkey(host, md5, MD5_KEYSIZE);
    offset_t of = key;

    {
        core::LockTryGuard lock(mutex_);
        if(!lock.isLocked()) { return -1; }

        DNSCacheData* shmptr = getShmPtr();
        if(nullptr == shmptr) { return -1; }

        int detect = 1;
        while(true) {
            if(shmptr->array[of].isUsed() && memcmp(md5, shmptr->array[of].getMd5Key(), MD5_KEYSIZE) == 0) {
                memcpy(ipaddr, shmptr->array[of].getAddr(), IPV4_ADDRSIZE);
                return 0;
            }
            if(detect >= HASH_MAXCONFLICT) {
                return -1;
            }
            detect ++;
            of ++;
            of = of % HASH_NODENUM;
        }
    }
    // Never be here
    return -1;
}

int DNSCache::updateDNS(const char* host, const char* ip, int& ttl) {
    if(nullptr == host || nullptr == ip) {
        return -1;
    }

    unsigned char md5[MD5_KEYSIZE] = {0};
    hashkey_t key = getHashkey(host, md5, MD5_KEYSIZE);
    offset_t of = key;

    {
        core::LockTryGuard lock(mutex_);
        if(lock.isLocked()) { return -1; }
        
        DNSCacheData* shmptr = getShmPtr();
        int detect = 1;
        int first = -1;
        while(true) {
            if(detect >= HASH_MAXCONFLICT) {
                break;
            }

            if(shmptr->array[of].isUsed()
               && memcmp(shmptr->array[of].getMd5Key(), md5, MD5_KEYSIZE) == 0) {
                if(shmptr->array[of].updateNode(md5, ip, ttl) != 0) { return -1; }
                shmptr->cachehitnum++;
                return 0;
            }

            if(!shmptr->array[of].isUsed() && -1 == first) {
                first = of;
            }

            of++;
            of = of % HASH_NODENUM;
        }

        if (-1 == first) {
            return -1;
        }

        if(shmptr->array[of].updateNode(md5, ip, ttl) != 0) { return -1; }    

        return 0;
    }

    return -1;
}


}


