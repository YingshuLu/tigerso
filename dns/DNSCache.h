#ifndef TS_NET_DNSCACHE_H_
#define TS_NET_DNSCACHE_H_

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>
#include "core/SysUtil.h"
#include "ssl/SSLHelper.h"

namespace tigerso::net {

#define CACHE_FILE_NAME "dnscache.dat"
#define MD5_KEYSIZE 16
#define IPV4_ADDRSIZE 16
#define HASH_NODENUM 65536
#define HASH_MAXCONFLICT 8

typedef unsigned short hashkey_t;
typedef unsigned int offset_t;

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

struct DNSNode {
    explicit DNSNode() {}

    inline bool isUsed() {
       time_t now = time(NULL);
       if(!reserved_ && expriedAt_ < now) {
            return false;
       }
       return true;
    }

    inline int updateNode(unsigned char* key, const char* ip, int& ttl) {
        if(nullptr == key || nullptr == ip) { return -1; }
        if(ttl < 0) {
            reserved_ = true;
        }
        else {
            expriedAt_ = time(NULL) + time_t(ttl);
        }
        memcpy(key_, key, MD5_KEYSIZE);
        memcmp(addr_, ip, IPV4_ADDRSIZE);
        return 0;
    }

    int setAddr(const char* addr, size_t len);
    int setExpiredAtTime(time_t);
    int setMd5Key(const char* key, size_t keylen);

    const char* getAddr();
    time_t getExpiredAtTime();
    const unsigned char* getMd5Key();
    
private:
    unsigned char key_[MD5_KEYSIZE] = {0};
    char addr_[IPV4_ADDRSIZE] = {0};
    time_t expriedAt_ = 0;
    bool reserved_ = false;
};

struct DNSCacheData {
    volatile size_t cachehitnum;
    DNSNode array[HASH_NODENUM];
};

class DNSCache {
public:
    static DNSCache* getInstance();
    int queryIP(const char*,  char*, size_t);
    int updateDNS(const char* host, const char* ip, int& ttl);
    ~DNSCache();

private:
    DNSCache();
    DNSCache(const DNSCache&);
    DNSCache& operator=(const DNSCache&);
    DNSCacheData* getShmPtr();
    
private:
    hashkey_t getHashkey(const std::string& host, unsigned char* pkey, size_t len) {
        unsigned char key[MD5_KEYSIZE] = {0};
        unsigned char* p = key;
        if(nullptr != pkey && len >= MD5_KEYSIZE) {
            p = pkey;
        }
        calcMd5(host.c_str(), p, MD5_KEYSIZE);
        return *((hashkey_t*) p) % HASH_NODENUM;
    }
       
private:
    static DNSCache* pInstance_;
    core::SharedMemory shm_;
    core::ShmMutex  mutex_; //process-shared mutex
    static std::string cachefile_; 
};

//net Global DNS cache
GLOBAL DNSCache* DNSCachePtr = DNSCache::getInstance();
}// namespace tigerso::net


#endif // TS_NET_DNSCACHE_H_
