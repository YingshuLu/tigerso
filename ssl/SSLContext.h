#ifndef TS_SSL_SSLCONTEXT_H_
#define TS_SSL_SSLCONTEXT_H_

#include <stdio.h>
#include <string.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <string>
#include "core/ConfigParser.h"
#include "core/tigerso.h"
#include "core/Logging.h"
#include "ssl/SSLHelper.h"

namespace tigerso {

typedef enum _SSL_ERROR_CODE {
    SCTX_ERROR_ERR = -1,
    SCTX_ERROR_OK
};

typedef enum _SSL_MODE_ {
    SCTX_MODE_SERVICE,
    SCTX_MODE_PROXY,
    SCTX_MODE_UNKNOWN
} SSLMode;

typedef enum _SSL_ROLE_{
    SCTX_ROLE_UNKNOWN = -1,
    SCTX_ROLE_CLIENT,
    SCTX_ROLE_SERVER
} SSLRole;

typedef enum _SSL_IO_STATE {
   SCTX_IO_ERROR = -1,
   SCTX_IO_RECALL,
   SCTX_IO_OK
}SSLIOState;

class SSLContext {
    
public:
    SSLContext(){}
    int init(SSLRole role);
    int bindSocket(int sockfd);
    int recv(void* buf, size_t len, size_t* readn);     
    int send(const void* buf, size_t len, size_t* written);
    int accept();
    int connect();
    int setupCertKey(X509* cert, EVP_PKEY* pkey);
    int close();
    bool active();
    void destory();
    ~SSLContext();

    int serrno = SSL_ERROR_NONE;
private:
    SSL* _ssl = nullptr;
    SSL_CTX* _sslctx = nullptr;
    X509* _peerCert = nullptr;
    X509* _ownCert = nullptr;
    SSLRole _role = SCTX_ROLE_UNKNOWN;
};

typedef int(*ServerCertVerifyCallback)(int, X509_STORE_CTX*);

namespace _OPENSSL_{
    void init();
    void destory();
}//namespace _OPENSSL_

}
#endif
