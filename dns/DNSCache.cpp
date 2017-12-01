#include "dns/DNSCache.h"

namespace tigerso::dns {
    
static int calcMd5(const char* buf, unsigned char* key, int keylen) {
    if(buf == nullptr || key == nullptr || keylen < MD5_KEYSIZE) {
        return -1;
    }

    MD5_CTX    mctx;
    MD5_Init(&mctx);
    MD5_Update(&mctx, (unsigned char*) buf, strlen(buf));
    MD5_Final(key, &mctx);
    return 0;
}

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

    /*Query stick DNS Nodes first*/
    std::string stickIP;
    if( getStickDNSNode(host, stickIP) == 0) { memcpy(ipaddr, (void*)stickIP.c_str(), stickIP.size()); return 0; }

    unsigned char md5[MD5_KEYSIZE] = {0};
    hashkey_t key = getHashkey(host, md5, MD5_KEYSIZE);

    std::cout << "query host: "<< host << ", hash key: " << key << std::endl;
    offset_t of = key;

    {
        core::LockTryGuard lock(mutex_);
        if(!lock.isLocked()) { return -1; }

        DNSCacheData* shmptr = getShmPtr();
        if(nullptr == shmptr) { return -1; }

        int cnt = 0;
        while(true) {
            if(isNodeUsed(shmptr->array[of]) && memcmp(host, shmptr->array[of].host_, strlen(host)) == 0) {
                memcpy(ipaddr, shmptr->array[of].addr_, IPV4_ADDRSIZE);
                std::cout << "Hit DNS cache" << std::endl;
                return 0;
            }

            of = getNextHashkey(md5, MD5_KEYSIZE, ++cnt);
            if( of == key ) {
                break;
            }
        }
    }
    // not found record in cache
    return -1;
}

int DNSCache::updateDNS(const char* host, const char* ip, int& ttl) {
    if(nullptr == host || nullptr == ip) {
        return -1;
    }

    unsigned char md5[MD5_KEYSIZE] = {0};
    hashkey_t key = getHashkey(host, md5, MD5_KEYSIZE);
    offset_t of = key;
    std::cout << "host: "<< host << ", hash key: " << key << std::endl;
    std::cout << "ip: "<< ip << std::endl;

    {
        core::LockTryGuard lock(mutex_);
        if(!lock.isLocked()) { return -1; }
        
        DNSCacheData* shmptr = getShmPtr();
        time_t todie_at = shmptr->array[key].expriedAt_;
        offset_t todie_of = key;
        int cnt = 0;

        while(true) {
//            std::cout << "node host: " << shmptr->array[of].host_ << ", query host: "<< host << std::endl;
            if(!isNodeUsed(shmptr->array[of]) || memcmp(shmptr->array[of].host_, host, strlen(host)) == 0) {
                if(updateNode(shmptr->array[of], host, md5, ip, ttl) != 0) { return -1; }
                std::cout << "update DNS cache" << std::endl;
                return 0;
            }

            if(shmptr->array[of].expriedAt_ < todie_at) {
                todie_of = of;
                todie_at = shmptr->array[of].expriedAt_;
            }

            of = getNextHashkey(md5, MD5_KEYSIZE, ++cnt);
            if(of == key) {
                break;
            }
        }

        //overwrite the oldest confilct node
        if(updateNode(shmptr->array[todie_of], host, md5, ip, ttl) != 0) { return -1; }    
        std::cout << "update DNS cache 2" << std::endl;
    }

    return 0;
}

int DNSCache::setStickDNSNode(std::string& host, std::string& ip) {
    stickDNSData_[host].push_back(ip);
    return 0;
}

int DNSCache::getStickDNSNode(const std::string& host, std::string& ip) {
    if(stickDNSData_.find(host) != stickDNSData_.end()) {
        /*Current, return the first ip record*/
        ip = stickDNSData_[host][1];
        return 0;
    }
    return -1;
}

hashkey_t DNSCache::getHashkey(const std::string& host, unsigned char* pkey, size_t len) {
    unsigned char key[MD5_KEYSIZE] = {0};
    unsigned char* p = key;
    if(nullptr != pkey && len >= MD5_KEYSIZE) {
        p = pkey;
    }
    calcMd5(host.c_str(), p, MD5_KEYSIZE);

    return getNextHashkey(pkey, MD5_KEYSIZE, 0);
    //return *((hashkey_t*) p) % HASH_NODENUM;
}

hashkey_t DNSCache::getNextHashkey(const unsigned char* pkey, const size_t len, const offset_t cnt) {
    offset_t start = cnt * 2;
    if(start > len || start +1 > len) {
        start = 0;
    }
    return *((hashkey_t*)(pkey+start)) % HASH_NODENUM;
}

bool DNSCache::isNodeUsed(DNSNode& node) {
    if(node.expriedAt_ < time(NULL))  { return false; }
    return true;
}

int DNSCache::updateNode(DNSNode& dst, const char* host, unsigned char* key, const char* ip, int& ttl) {
    if(nullptr == key || nullptr == ip) { return -1; }

    dst.expriedAt_ = time(NULL) + time_t(ttl);
    memcpy(dst.host_, host, strlen(host));
    memcpy(dst.key_, key, MD5_KEYSIZE);
    memcpy(dst.addr_, ip, IPV4_ADDRSIZE);
    return 0;
}

}


