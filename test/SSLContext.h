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

#define SSLStrerror() ::ERR_error_string(ERR_get_error(), NULL) 
#define HTTPS_CERT_VERIFY_MAX_DEPTH 9

typedef enum {
    HTTPS_ERROR_ERR = -100,
    HTTPS_ERROR_OK = 0
} HTTPSErrorType;

typedef enum {
    HTTPS_MODE_SERVICE;
    HTTPS_MODE_PROXY;
    HTTPS_MODE_UNKNOWN;
} HttpMode;

typedef enum _SSL_ROLE_{
    HTTPS_ROLE_UNKNOWN = -1,
    HTTPS_ROLE_CLIENT,
    HTTPS_ROLE_SERVER
} HtttsRole;


typedef enum HTTPS_IO_STATE {
    HTTPS_IO_ERROR = -1,
    HTTPS_IO_OK,
    HTTPS_IO_RECALL,
};

class SSLContext {
    
public:
    HttpsContext();
    int init(int role);
    int bindSocket(int sockfd);
    int recv(void* buf, size_t len, size_t* readn);     
    int send(void* buf, size_t len, size_t* written);
    int accept();
    int connect();
    int setupCertKey(X509* cert, EVP_PKEY* pkey);
    int close();
private:
    SSL* _ssl = nullptr;
    SSL_CTX* _sslctx = nullptr;
    X509* _peerCert = nullptr;
    X509* _ownCert = nullptr;
    HttpsRole _role = SCTX_ROLE_UNKNOWN;
};

typedef int(*ServerCertVerifyCallback)(int, X509_STORE_CTX*);


#endif
