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
};

struct DNSCacheData {
    volatile size_t cachehitnum;
    DNSNode array[HASH_NODENUM];
};

class DNSCache {
public:
    DNSCache* getInstance();
    int findAddr(const char*,  char*, size_t);
    ~DNSCache();

private:
    DNSCache();
    DNSCache(const DNSCache&);
    DNSCache& operator=(const DNSCache&);
    DNSCacheData* getShmPtr();
    DNSNode* findNode(const char*);
    
private:
    static DNSCache* pInstance_;
    core::SharedMemory shm_;
    core::ShmMutex  mutex_; //process-shared mutex
    static std::string cachefile_; 
};

}// namespace tigerso::net


#endif // TS_NET_DNSCACHE_H_
