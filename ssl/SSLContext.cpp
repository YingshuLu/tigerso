#include <stdio.h>
#include <string.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <string>
#include "core/ConfigParser.h"
#include "core/Logging.h"
#include "core/tigerso.h"
#include "ssl/SSLHelper.h"
#include "ssl/SSLContext.h"

namespace tigerso {

#define SSLStrerror() ::ERR_error_string(ERR_get_error(), NULL) 

namespace _OPENSSL_ {

#define SCTX_CERT_VERIFY_MAX_DEPTH 9
static bool OPENSSL_INITIZED = false;
static SSL_CTX* g_client_ssl_ctx = NULL;
static SSL_CTX* g_server_ssl_ctx = NULL;
static ServerCertVerifyCallback g_server_cert_verify_cb = NULL;
ConfigParser* g_config = ConfigParser::getInstance(); 

    void _initOpenssl() {
        if(!OPENSSL_INITIZED) {
            ::SSL_load_error_strings();
            ::OpenSSL_add_ssl_algorithms();
            OPENSSL_INITIZED = true;
        }
        return;
    }

    void _uinitClientContext() {
        if(g_client_ssl_ctx) { SSL_CTX_free(g_client_ssl_ctx); }
        return;
    }

    void _uinitServerContext() {
        if(g_server_ssl_ctx) { SSL_CTX_free(g_server_ssl_ctx); }
        return;
    }  

    void _destoryOpenssl() {
        _uinitClientContext();
        _uinitServerContext();
        EVP_cleanup();
        OPENSSL_INITIZED = false;
        return;
    }

    /* 0: error, 1: pass verification*/
    int _clientVerifyServerCertCallback(int ok, X509_STORE_CTX* xstore) {
        return 1;
    }

    int _initClientContext(const char* trustCAPath, const char* crlPath) {
        _initOpenssl();
        g_client_ssl_ctx = ::SSL_CTX_new(SSLv23_client_method());

        if(!g_client_ssl_ctx) {
            DBG_LOG("SSL CTX New error: %s\n", SSLStrerror());
            _uinitClientContext();
            return SCTX_ERROR_ERR;
        }

        /*Client SSL CTX configure*/
        {
            //Set Cipher, all cipher suits by default
            ::SSL_CTX_set_cipher_list(g_client_ssl_ctx, "ALL");

            //Set non-blocking mode for SSL_write
            ::SSL_CTX_set_mode(g_client_ssl_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE| SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

            //Set server cert verify callback
            ::SSL_CTX_set_verify(g_client_ssl_ctx, SSL_VERIFY_PEER, g_server_cert_verify_cb);
            ::SSL_CTX_set_verify_depth(g_client_ssl_ctx, SCTX_CERT_VERIFY_MAX_DEPTH + 1);

            //Not support for SSL seesion recover
            ::SSL_CTX_set_session_cache_mode(g_client_ssl_ctx, SSL_SESS_CACHE_OFF);

            if(::SSL_CTX_load_verify_locations(g_client_ssl_ctx, NULL, trustCAPath) != 0) {
                DBG_LOG("Error! failed to load trust CA Path: %s, reason: %s\n", trustCAPath, SSLStrerror());
                _uinitClientContext();
                return SCTX_ERROR_ERR;
            }

            X509_STORE* store = SSL_CTX_get_cert_store(g_client_ssl_ctx);
            X509_LOOKUP* lookup;
            if (!(lookup = X509_STORE_add_lookup(store, X509_LOOKUP_hash_dir()))) { DBG_LOG("Error creating X509_LOOKUP object, reason: %s\n", SSLStrerror()); return SCTX_ERROR_ERR; }

            if (X509_LOOKUP_add_dir(lookup, crlPath, X509_FILETYPE_DEFAULT) != 1)  { DBG_LOG("Error reading the CRL dir, reason: %s\n", SSLStrerror()); return SCTX_ERROR_ERR; }
            X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
        }
        return SCTX_ERROR_OK;
    }

    int _initServerContext(const char* servercert, const char* privatekey) {
        _initOpenssl();
        g_server_ssl_ctx = ::SSL_CTX_new(SSLv23_server_method());

        if(!g_server_ssl_ctx) {
            DBG_LOG("SSL CTX New error: %s\n", SSLStrerror());
            _uinitServerContext();
            return SCTX_ERROR_ERR;
        }
        /*Server SSL CTX cinfigure, we will set Cert and privateKey on SSL handle*/
        {
            //Set Cipher, all cipher suits by default
            ::SSL_CTX_set_cipher_list(g_server_ssl_ctx, "ALL");

            //Set non-blocking mode for SSL_write
            ::SSL_CTX_set_mode(g_server_ssl_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE| SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

            //Not support for SSL seesion recover
            ::SSL_CTX_set_session_cache_mode(g_client_ssl_ctx, SSL_SESS_CACHE_OFF);

            SSL_CTX_set_ecdh_auto(g_server_ssl_ctx, 1);

            /* Set the key and cert */
            if (SSL_CTX_use_certificate_file(g_server_ssl_ctx, servercert, SSL_FILETYPE_PEM) <= 0) {
                DBG_LOG("server SSL context failed to certificate, reason: %s\n", SSLStrerror());
                return SCTX_ERROR_ERR;
            }

            if (SSL_CTX_use_PrivateKey_file(g_server_ssl_ctx, privatekey, SSL_FILETYPE_PEM) <= 0 ) {
                DBG_LOG("server SSL context failed to private key, reason: %s\n", SSLStrerror());
                return SCTX_ERROR_ERR;
            }
        }

       return SCTX_ERROR_OK;
    }

   SSL_CTX* getClientSSLCTX() {
        if(!g_client_ssl_ctx) {
            std::string capath = g_config->getValueByKey("proxy", "truststore");
            std::string crlpath = g_config->getValueByKey("proxy", "crlpath");
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
            if(!certfile.size() || !keyfile.size()) {
                INFO_LOG("Failed to get cert or key file");
                return NULL;
            }
            _initServerContext(certfile.c_str(), keyfile.c_str());
        }
        return g_server_ssl_ctx;
    }

    void init() {
        _initOpenssl();
        return;
    }

    void destory() {
        _destoryOpenssl();
        return;
    }

}//namespace _OPENSSL_


int SSLContext::init(SSLRole role) {
        _role = role;
        if(SCTX_ROLE_CLIENT == _role) {
            _ssl= ::SSL_new(_OPENSSL_::getClientSSLCTX());
        }
        else if(SCTX_ROLE_SERVER == _role) {
            _ssl = ::SSL_new(_OPENSSL_::getServerSSLCTX());
        }
        else {
            _role = SCTX_ROLE_UNKNOWN;
            return SCTX_ERROR_ERR;
        }
        return SCTX_ERROR_OK;
    }

int SSLContext::bindSocket(int sockfd) {
    if(nullptr == _ssl) {
        return SCTX_ERROR_ERR;
    }
    if(::SSL_set_fd(_ssl, sockfd) != 1) {
        DBG_LOG("SSL set socket failed: %s", SSLStrerror());
        destory();
        return SCTX_ERROR_ERR;
    }
    return SCTX_ERROR_OK;
}


int SSLContext::recv(void* buf, size_t len, size_t* readn) {
    int sockfd = ::SSL_get_fd(_ssl);
    if(!validFd(sockfd)) {
        INFO_LOG("SSL get socket failed: %s", SSLStrerror());
        return SCTX_IO_ERROR;
    }
    
    if(nonBlocking(sockfd) != 0) {
        INFO_LOG("non-blocking setting failed: %d, :%s", errno, strerror(errno));
        return SCTX_IO_ERROR;
    }

    int ret = ::SSL_read(_ssl, buf, len);
    serrno = ::SSL_get_error(_ssl, ret);
    if(ret > 0 ) {
        *readn = ret;
        return SCTX_IO_OK;
    }
    else {
        if(SSL_ERROR_WANT_READ == serrno || SSL_ERROR_WANT_WRITE == serrno) {
            INFO_LOG("SSL need more IO process");
            return SCTX_IO_RECALL;
        }
    }
    destory();
    INFO_LOG("SSL recv failed: %s", SSLStrerror());
    return SCTX_IO_ERROR;
}

int SSLContext::send(const void* buf, size_t len, size_t* written) {
    int sockfd = ::SSL_get_fd(_ssl);
    if(!validFd(sockfd)) {
        INFO_LOG("SSL get socket failed: %s", SSLStrerror());
        destory();
        return SCTX_IO_ERROR;
    }
    
    if(nonBlocking(sockfd) != 0) {
        INFO_LOG("non-blocking setting failed: %d, :%s", errno, strerror(errno));
        return SCTX_IO_ERROR;
    }

    int ret = ::SSL_write(_ssl, buf, len);
    serrno = ::SSL_get_error(_ssl, ret);
    if(ret > 0 ) {
        *written = ret;
        return SCTX_IO_OK;
    }
    else {
        if(SSL_ERROR_WANT_READ == serrno || SSL_ERROR_WANT_WRITE == serrno) {
            *written = 0;
            INFO_LOG("SSL need more IO process");
            return SCTX_IO_RECALL;
        }
    }
    destory();
    INFO_LOG("SSL send failed: %d, :%s", errno, strerror(errno));
    return SCTX_IO_ERROR;
}

int SSLContext::accept() {
    int ret = ::SSL_accept(_ssl);
    serrno = ::SSL_get_error(_ssl, ret);
    if(ret != 1) {
        if(0 == ret) {
            INFO_LOG("SSL accept client failed: %s", SSLStrerror());
            destory();
            return SCTX_IO_ERROR;
        }
        else {
            if(SSL_ERROR_WANT_WRITE == serrno || SSL_ERROR_WANT_READ == serrno) {
                INFO_LOG("SSL need more IO process");
                return SCTX_IO_RECALL;
            }
            INFO_LOG("SSL accept client failed: %s", SSLStrerror());
            destory();
            return SCTX_IO_ERROR;
        }
    }
    return SCTX_IO_OK;
}

int SSLContext::connect() {
   int ret = ::SSL_connect(_ssl); 
   serrno = ::SSL_get_error(_ssl, ret);
   if(ret != 1) {
        if(0 == ret) {
            INFO_LOG("SSL connect failed: %s", SSLStrerror());
            destory();
            return SCTX_IO_ERROR;
        }
        else {
            if(SSL_ERROR_WANT_WRITE == serrno || SSL_ERROR_WANT_READ == serrno) {
                INFO_LOG("SSL need more IO process");
                return SCTX_IO_RECALL;
            }
            INFO_LOG("SSL connect failed: %s", SSLStrerror());
            destory();
            return SCTX_IO_ERROR;
        }
    }
    return SCTX_IO_OK;
}

int SSLContext::setupCertKey(X509* cert, EVP_PKEY* pkey) {
    int ret = 0;
    ret = ::SSL_use_certificate(_ssl, cert);
    serrno = ::SSL_get_error(_ssl, ret);
    if(1 != ret) {
        INFO_LOG("setup certificate failed: %s", SSLStrerror());
        destory();
        return SCTX_ERROR_ERR;
    }

    ret = ::SSL_use_PrivateKey(_ssl, pkey);
    serrno = ::SSL_get_error(_ssl, ret);
    if(1 != ret) {
        INFO_LOG("setup private key failed: %s", SSLStrerror());
        destory();
        return SCTX_ERROR_ERR;
    }

    return SCTX_ERROR_OK;
}

int SSLContext::close() {
    if(active()) { 
        int ret = SSL_shutdown(_ssl);
        serrno = ::SSL_get_error(_ssl, ret);
        if(1 != ret) {
            if(SSL_ERROR_WANT_READ == serrno || SSL_ERROR_WANT_WRITE == serrno) {
                return SCTX_IO_RECALL;
            }
            destory();
            return SCTX_IO_ERROR;
        }
    }
    SSL_free(_ssl);
    _ssl = nullptr;
    return SCTX_IO_OK;
}

void SSLContext::destory() {
    if(active()) {
        int ret = SSL_shutdown(_ssl);
        serrno = ::SSL_get_error(_ssl, ret);
    }
     SSL_free(_ssl);
    _ssl = nullptr;
}

SSLContext::~SSLContext() {
    destory();
}

bool SSLContext::active() {
    return SSLHelper::validSSL(_ssl);
}

}//namespace tigerso

