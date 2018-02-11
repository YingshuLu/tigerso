#ifndef TS_SSL_CERTCACHE_H_
#define TS_SSL_CERTCACHE_H_

#include "core/Hash.h"
#include "core/SysUtil.h"
#include "core/FileLock.h"

extern "C" {
#include "openssl/ssl.h"
#include "openssl/crypto.h"
#include "openssl/bio.h"
#include "openssl/pem.h"
#include "openssl/evp.h"
#include "openssl/x509.h"
#include "openssl/x509v3.h"
#include "openssl/rsa.h"
#include "openssl/rand.h"
#include "openssl/bn.h"
#include "openssl/err.h"
#include "openssl/engine.h"
#include "openssl/md5.h"
}

namespace tigerso {

class CertCache {

public:
    static CertCache* getInstance();
    int query(X509* orig, X509** cert, EVP_PKEY** pkey);
    int store(X509* orig, X509* cert, EVP_PKEY* pkey);
    ~CertCache();

private:
    CertCache();
    int getKeyName(X509* orig, char* keyname, int len);
    HashKey generateHashKey(const char* keyname);
    int hashSearch(X509* orig, struct CertNode**);
    
private:
    SharedMemory certstore_;
    FileLock lock_;
    pid_t parentPid_ = -1;
    static CertCache* pcache_;
};

} //namespace tigerso

#endif
