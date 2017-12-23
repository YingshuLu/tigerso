#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <openssl/ssl.h>

#define GLOBAL

//SSLv1.0 SSLv2.0 has been discarded by default as their insecure
typedef enum _SSL_version {
    SSL_VERSION_SSLV3,
    SSL_VERSION_TLS10,
    SSL_VERSION_TLS11,
    SSL_VERSION_TLS12,
    SSL_VERSION_ALL,
    SSL_VERSION_UNKNOW

} SSLVersion;


class SSLContextFactory {

public:

    ~SSLContextFactory() {
        destory();
    }

private:
    SSL_CTX* newServerContext(const char* cert, const char* pkey, SSLVersion version);
    SSL_CTX* newClientContext(const char* certpath, const char* crlpath, SSLVersion version);

    void initilize() {
        for(int v = SSL_VERSION_SSLV3; v < SSL_VERSION_UNKNOW; v++) {
            serverContexts[v] = newServerContext(v);
            clientContexts[v] = newClientContext(v);
        }

    }
    void destory() {
        for(int v = SSL_VERSION_SSLV3; v < SSL_VERSION_UNKNOW; v++) {
            SSL_CTX_free(serverContexts[v]);
            SSL_CTX_free(clientContexts[v]);
        }
    }
private:
    SSL_CTX* serverContexts[SSL_VERSION_UNKNOW];
    SSL_CTX* clientContexts[SSL_VERSION_UNKNOW];

    std::string serverCertfile;
    std::string 

};
