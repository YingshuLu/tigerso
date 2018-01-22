#ifndef TS_NET_DNSCACHE_H_
#define TS_NET_DNSCACHE_H_

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include "core/SysUtil.h"
#include "ssl/SSLHelper.h"

namespace tigerso {

#define CACHE_FILE_NAME "dnscache.dat"
#define MD5_KEYSIZE 16
#define IPV4_ADDRSIZE 16
#define HASH_NODENUM 65536
#define HASH_MAXCONFLICT 8
#define HOST_MAXLENGTH 256

typedef unsigned short hashkey_t;
typedef unsigned int offset_t;

struct DNSNode {
    char host_[HOST_MAXLENGTH] = {0};
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
    static void init();
    static DNSCache* getInstance();
    int queryIP(const char*,  char*, size_t);
    int updateDNS(const char* host, const char* ip, int& ttl);
    int setStickDNSNode(std::string& host, std::string& ip);
    int getStickDNSNode(const std::string& host, std::string& ip);
    ~DNSCache();

private:
    DNSCache();
    DNSCache(const DNSCache&);
    DNSCache& operator=(const DNSCache&);
    DNSCacheData* getShmPtr();
    
private:
    hashkey_t getHashkey(const std::string& host, unsigned char* pkey, size_t len);
    hashkey_t getNextHashkey(const unsigned char* pkey, const size_t len, const offset_t cnt);
    bool isNodeUsed(DNSNode& node);
    int updateNode(DNSNode& dst, const char* host, unsigned char* key, const char* ip, int& ttl);
    int store2Memory(const char* host, const char* ip, int ttl);
    int memory2Shared();
    int DNS2Shared(const char* host, const char* ip, int ttl);
       
private:
    static std::unique_ptr<DNSCache> pInstance_;
    SharedMemory shm_;
    ShmMutex  mutex_; //process-shared mutex
    std::map<std::string, std::vector<std::string>> stickDNSData_;
    std::map<std::string, std::pair<std::string, time_t>> shmCache_;
    static std::string cachefile_; 
};

//net Global DNS cache
}// namespace tigerso::dns


#endif // TS_NET_DNSCACHE_H_
