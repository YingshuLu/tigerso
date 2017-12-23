#include <stdio.h>
#include <string.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <string>
#include "core/ConfigParser.h"
#include "core/tigerso.h"
#include "ssl/SSLHelper.h"

namespace SSLContext {

    static SSL_CTX* g_client_ssl_ctx = NULL;
    static SSL_CTX* g_server_ssl_ctx = NULL;
    static serverCertVerifyCallback g_server_cert_verify_cb = NULL;
    ConfigParser* g_config = ConfigParser::getInstance(); 
    
    SSL_CTX* getClientSSLCTX() {
        if(!g_client_ssl_ctx) {
            std::string capath = g_config->getValueByKey("proxy", "truststore");
            std::string crlpath = g_config->getValueByKey("proxy", "crlPath");
            if(!capath.size() || !crlpath.size()) {
                INFO_LOG("Failed to get CA or CRL store");
                return NULL;
            }
            _initClientContext(capath.c_str(), crlpath.c_str());
        }
        return g_client_ssl_ctx;
    }

    SSL_CTX* getServerSSLCTX() {
        if(!g_server_ssl_ctx) {
            std::string certfile = g_config->getValueByKey("http", "cert");
            std::string keyfile = g_config->getValueByKey("http", "pkey");
            if(!certfile.size() || !key.size()) {
                INFO_LOG("Failed to get cert or key file");
                return NULL;
            }
            _initServerContext(certfile.c_str(), keyfile.c_str());
        }
        return g_server_ssl_ctx;
    }

    
    /* 0: error, 1: pass verification*/
    int _clientVerifyServerCertCallback(int ok, X509_STORE_CTX* xstore) {

        return 1;
    }


    void _uinitClientContext() {
        SSL_CTX_free(g_client_ssl_ctx);
    }

    void _uinitServerContext() {
        SSL_CTX_free(g_server_ssl_ctx);
    }

    int _initClientContext(const char* trustCAPath, const char* crlPath) {
        g_client_ssl_ctx = ::SSL_CTX_new(SSLv23_client_method());
        g_server_ssl_ctx = ::SSL_CTX_new(SSLv23_server_method());

        if(!g_client_ssl_ctx || g_server_ssl_ctx) {
            DBG_LOG("SSL CTX New error: %s\n", SSLStrerror());
            _uinitClientContext();
            return HTTPS_ERROR_ERR;
        }

        /*Client SSL CTX configure*/
        {
            //Set Cipher, all cipher suits by default
            ::SSL_CTX_set_cipher_list(g_client_ssl_ctx, "ALL");

            //Set non-blocking mode for SSL_write
            ::SSL_CTX_set_mode(g_client_ssl_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE| SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

            //Set server cert verify callback
            ::SSL_CTX_set_verify(g_client_ssl_ctx, SSL_VERIFY_PEER, g_server_cert_verify_cb);
            ::SSL_CTX_set_verify_depth(g_client_ssl_ctx, HTTPS_CERT_VERIFY_MAX_DEPTH + 1);

            //Not support for SSL seesion recover
            ::SSL_CTX_set_session_cache_mode(g_client_ssl_ctx, SSL_SESS_CACHE_OFF);

            if(::SSL_CTX_load_verify_locations(g_client_ssl_ctx, NULL, trustCAPath) != 0) {
                DBG_LOG("Error! failed to load trust CA Path: %s, reason: %s\n", trustCAPath, SSLStrerror());
                _uinitClientContext();
                return HTTPS_ERROR_ERRl
            }

            X509_STORE* store = SSL_CTX_get_cert_store(g_client_ssl_ctx);
            X509_LOOKUP* lookup;
            if (!(lookup = X509_STORE_add_lookup(store, X509_LOOKUP_hash_dir()))) { DBG_LOG("Error creating X509_LOOKUP object, reason: %s\n", SSLStrerror()); return HTTPS_ERROR_ERR; }

            if (X509_LOOKUP_add_dir(lookup, crlPath, X509_FILETYPE_DEFAULT) != 1)  { DBG_LOG("Error reading the CRL dir, reason: %s\n", SSLStrerror()); return HTTPS_ERROR_ERR; }
            X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
        }
        return HTTPS_ERROR_OK;
    }

    int _initServerContext(const char* servercert, const char* privatekey) {

        /*Server SSL CTX cinfigure, we will set Cert and privateKey on SSL handle*/
        {
            //Set Cipher, all cipher suits by default
            ::SSL_CTX_set_cipher_list(g_server_ssl_ctx, "ALL");

            //Set non-blocking mode for SSL_write
            ::SSL_CTX_set_mode(g_server_ssl_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE| SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

            //Not support for SSL seesion recover
            ::SSL_CTX_set_session_cache_mode(g_client_ssl_ctx, SSL_SESS_CACHE_OFF);

            SSL_CTX_set_ecdh_auto(ctx, 1);

            /* Set the key and cert */
            if (SSL_CTX_use_certificate_file(g_server_ssl_ctx, servercert, SSL_FILETYPE_PEM) <= 0) {
                DBG_LOG("server SSL context failed to certificate, reason: %s\n", SSLStrerror());
                return HTTPS_ERROR_ERR;
            }

            if (SSL_CTX_use_PrivateKey_file(g_server_ssl_ctx, privatekey, SSL_FILETYPE_PEM) <= 0 ) {
                DBG_LOG("server SSL context failed to private key, reason: %s\n", SSLStrerror());
                return HTTPS_ERROR_OK;
            }

            return HTTPS_ERROR_OK;
    }
    
    

}

class HttpsContext {
    
public:
    HttpsContext():  {}

    int init(int role) {
        if(HTTPS_ROLE_CLIENT == _role) {
             _role = role;
            return ::SSL_new(SSLContext::getClientSSLCTX());
        }
        else if(HTTPS_ROLE_SERVER == _role) {
             _role = role;
            return ::SSL_new(SSLContext::getServerSSLCTX());
        }
       
        return HTTPS_ERROR_ERR;
    }

    int bindSocket(int sockfd) {
        if(nullptr == _ssl) {
            return HTTPS_ERROR_ERR;
        }
        if(::SSL_set_fd(_ssl, sockfd) != 1) {
            DBG_LOG("SSL set socket failed: %s", SSLStrerror());
            return HTTPS_ERROR_ERR;
        }
        return HTTPS_ERROR_OK;
    }


    int recv(void* buf, size_t len, size_t* readn) {
        int sockfd = ::SSL_get_fd(_ssl);
        if(validFd(sockfd)) {
            INFO_LOG("SSL get socket failed: %s", SSLStrerror());
            return HTTPS_IO_ERROR;
        }
        
        if(nonBlocking(sockfd) != 0) {
            INFO_LOG("non-blocking setting failed: %d, :%s", errno, strerror(errno));
            return HTTPS_IO_ERROR;
        }

        int ret = ::SSL_read_ex(_ssl, buf, len, readn);
        if(ret > 0 ) {
            *readn = rn;
            return HTTPS_IO_OK;
        }
        else {
            int code = ::SSL_get_error();
            if(SSK_ERROR_WANT_READ == code || SSL_ERROR_WANT_WRITE == code) {
                return HTTPS_IO_RECALL;
            }
        }
        return HTTPS_IO_ERROR;
    }

    int send(void* buf, size_t len, size_t* written) {
        int sockfd = ::SSL_get_fd(_ssl);
        if(validFd(sockfd)) {
            INFO_LOG("SSL get socket failed: %s", SSLStrerror());
            return HTTPS_IO_ERROR;
        }
        
        if(nonBlocking(sockfd) != 0) {
            INFO_LOG("non-blocking setting failed: %d, :%s", errno, strerror(errno));
            return HTTPS_IO_ERROR;
        }

        int ret = ::SSL_write_ex(_ssl, buf, len, written);
        if(ret > 0 ) {
            return HTTPS_IO_OK;
        }
        else {
            int code = ::SSL_get_error();
            if(SSK_ERROR_WANT_READ == code || SSL_ERROR_WANT_WRITE == code) {
                return HTTPS_IO_RECALL;
            }
        }
        return HTTPS_IO_ERROR;
    }

    int accept() {
        int ret = ::SSL_accept(_ssl);
        if(ret != 1) {
            if(0 == ret) {
                INFO_LOG("SSL accept client failed: %s", SSLStrerror());
                return HTTPS_IO_ERROR;
            }
            else {
                int code = ::SSL_get_error();
                if(SSL_ERROR_WANT_WRITE == code || SSL_ERROR_WANT_READ == code) {
                    return HTTPS_IO_RECALL;
                }
                INFO_LOG("SSL accept client failed: %s", SSLStrerror());
                return HTTPS_IO_ERROR;
            }
        }
        return HTTPS_IO_OK;
    }

    int connect() {
       int ret = ::SSL_connect(_ssl); 
       if(ret != 1) {
            if(0 == ret) {
                INFO_LOG("SSL connect failed: %s", SSLStrerror());
                return HTTPS_IO_ERROR;
            }
            else {
                int code = ::SSL_get_error();
                if(SSL_ERROR_WANT_WRITE == code || SSL_ERROR_WANT_READ == code) {
                    return HTTPS_IO_RECALL;
                }
                INFO_LOG("SSL connect failed: %s", SSLStrerror());
                return HTTPS_IO_ERROR;
            }
        }
        return HTTPS_IO_OK;
    }

    int setupCertKey(X509* cert, EVP_PKEY* pkey) {
        int ret = 0;
        ret = ::SSL_use_certificate(_ssl, cert);
        if(1 != ret) {
            INFO_LOG("setup certificate failed: %s", SSLStrerror());
            return HTTPS_ERROR_ERR;
        }

        ret = ::SSL_use_PrivateKey(_ssl, pkey);
        if(1 != ret) {
            INFO_LOG("setup private key failed: %s", SSLStrerror());
            return HTTPS_ERROR_ERR;
        }

        return HTTPS_ERROR_OK;
    }

    int close() {
        int ret = SSL_shutdown(_ssl);
        if(1 != ret) {
            int code = ::SSL_get_error();
            if(SSL_ERROR_WANT_READ == code || SSL_ERROR_WANT_WRITE == code) {
                return HTTPS_IO_RECALL;
            }
            return HTTPS_IO_ERROR;
        }
        SSL_free(_ssl);
        _ssl = nullptr;
        return HTTPS_IO_OK;
    }

    ~HttpsContext() {
        this->close();
    }

    bool active() {
        return _ssl != nullptr;
    }

private:
    SSL* _ssl = nullptr;
    SSL_CTX* _sslctx = nullptr;
    X509* _peerCert = nullptr;
    X509* _ownCert = nullptr;
    HttpsRole _role = HTTPS_ROLE_UNKNOWN;
};

typedef int(*ServerCertVerifyCallback)(int, X509_STORE_CTX*);


