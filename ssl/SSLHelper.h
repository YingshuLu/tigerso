#ifndef TS_SSL_SSLHELPER_H_
#define TS_SSL_SSLHELPER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "core/BaseClass.h"

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
#define GOBAL_DEFAULT_RSA_KEY_LENGTH 2048

#define HTTPS_IO_RECALL 0
#define HTTPS_IO_ERROR -1

X509* loadX509FromFile(const char* filename);
int storeX509ToPEMStr(X509* cert, char* buf, int len);
EVP_PKEY* loadPrivateKeyFromFile(const char* keyfile, const char* passwd);

class SSLHelper {

public:
/*
 Input:
 ca_cert: CA Certificate
 ca_pkey: CA private key
 key_length: resigned certificate key length
 org_cert: orginial certificate

 Output:
 cert: resigned certificate
 pkey: resigned certificate private key
 
 Success: retuen true
 Failure: return false
*/
static bool signCert(X509* ca_cert, EVP_PKEY* ca_pkey, int key_length, X509* org_cert, X509** cert, EVP_PKEY** pkey);
static  bool validSSL(SSL* ssl);
static int MD5(const char* input, char* output, int len);

};

}
#endif
