#include "core/Logging.h"
#include "core/Dechex.h"
#include "core/FileLock.h"
#include "ssl/CertCache.h"
#include "ssl/SSLHelper.h"

namespace tigerso {

#define MAX_CERT_LENGTH  8192
#define MAX_KEY_LENGTH   8192
#define CERTSTORE        "resigned-certstore"

struct CertNode {
    char keyname[64] = {0};
    unsigned short certLen = 0;
    unsigned short pkeyLen = 0;
    unsigned char cert [MAX_CERT_LENGTH] = {0};
    unsigned char pkey [MAX_KEY_LENGTH] = {0};
};

static const HashKey CERTSTORE_SIZE = Hash::adjustHashSize(2048);

CertCache* CertCache::pcache_ = nullptr;

CertCache* CertCache::getInstance() {
    if(!pcache_) {
        pcache_ = new CertCache;
    }
    return pcache_;
}

CertCache::~CertCache() {

}

int CertCache::query(X509* orig, X509** cert, EVP_PKEY** pkey) {
    *cert = NULL;
    *pkey = NULL;
    CertNode* node = NULL;
    
    //no cache
    if(hashSearch(orig, &node) != 0) { return -1; }

    const unsigned char* node_cert = node->cert;
    if(0 == node->certLen || NULL == d2i_X509(cert, &node_cert, node->certLen)) {
        INFO_LOG("query: failed to convert certificate");
        return -1;
    }

    const unsigned char* node_pkey = node->pkey;
    if(0 == node->pkeyLen || NULL == d2i_AutoPrivateKey(pkey, &node_pkey, node->pkeyLen)) {
        INFO_LOG("query: failed to convert privateKey");
        return -1;
    }

    return 0;
}

//Not update resigned certificate, if it was ever stored
int CertCache::store(X509* orig, X509* cert, EVP_PKEY* pkey) {
    CertNode* node = NULL;
    
    //has been in cache
    if(hashSearch(orig, &node) == 0) { return 0; }

    node->certLen = i2d_X509(cert, NULL);
    node->pkeyLen = i2d_PrivateKey(pkey, NULL);

    INFO_LOG("resigned certificate length: %u, private key length: %u", node->certLen, node->pkeyLen);
    if(node->certLen > MAX_CERT_LENGTH || node->certLen < 0) { return -1; }
    if(node->pkeyLen > MAX_KEY_LENGTH || node->pkeyLen < 0) { return -1; }

    unsigned char* buffer = node->cert;
    i2d_X509(cert, &buffer);
    
    buffer = node->pkey;
    i2d_PrivateKey(pkey, &buffer);

    {
        LockGuard Lock(lock_);
        getKeyName(orig, node->keyname, sizeof(node->keyname));
    }

    return 0;
}

CertCache::CertCache(): 
    certstore_(CERTSTORE, CERTSTORE_SIZE * sizeof(CertNode)),
    lock_(std::string("/dev/shm/.").append(CERTSTORE) + ".lock") {

}

//Issuer and serial number can loacte a specific End certificate
int CertCache::getKeyName(X509* orig, char* keyname, int keylen) {
    if(!orig || !keyname) {
        INFO_LOG("certificate or keyname is NULL");
        return -1;
    }

    unsigned int length = 0;
    X509_NAME* issuer = orig->cert_info->issuer;
    ASN1_INTEGER* serialNumber = orig->cert_info->serialNumber;

    int len = i2d_X509_NAME(issuer, NULL);
    if(len < 0) {
        INFO_LOG("failed to get certificate issuer");
        return -1;
    }
    length += len;
    len = i2d_ASN1_INTEGER(serialNumber, NULL);
    if(len < 0) {
        INFO_LOG("failed to get certificate serialNumber");
        return -1;
    }
    length += len;

    if(length > 65535 || !length) {
        INFO_LOG("certificate is too big/short, failed to get information");
        return -1;
    }

    const char* hexstr = dec2hex(length);
    unsigned int hexstr_len = strlen(hexstr);

    length += 1;
    char* buffer = (char*)malloc(length);
    buffer[length] = '\0';

    char* bufptr = buffer;
    char* cur = keyname;

    strncpy(cur, hexstr, hexstr_len);
    cur += hexstr_len;
    *(cur) = '-';
    cur++;

    i2d_X509_NAME(issuer, (unsigned char**)&bufptr);
    i2d_ASN1_INTEGER(serialNumber, (unsigned char**)&bufptr);
    SSLHelper::MD5((const char*)(buffer), (char*)(cur), keylen - hexstr_len - 1); 
    DBG_LOG("get cert keyname: %s", keyname);
    free(buffer);

    return 0;
}

HashKey CertCache::generateHashKey(const char* keyname) {
    return Hash::hash(keyname, CERTSTORE_SIZE);
}

int CertCache::hashSearch(X509* orig, CertNode** node) {
    CertNode* nodes = (CertNode*)certstore_.get_shm_ptr();
    if(!nodes) {
        INFO_LOG("failed to get certstore shared memory");
        return -1;
    }

    char keyname[64] = {0};
    int ret = getKeyName(orig, keyname, sizeof(keyname));
    if(ret != 0) { return -1; }
    
    HashKey key = generateHashKey(keyname);
    int knlen = strlen(keyname);
    *node = &(nodes[key]);

    {
        lock_.read_lock();
        ret = strncmp(keyname, nodes[key].keyname, knlen); 
        lock_.unlock();
    }

    if(ret != 0) { return -1; }
    return 0;
}

}//namespace tigerso
