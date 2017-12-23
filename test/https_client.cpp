#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>;
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "core/SysUtil.h"
#include "http/HttpMessage.h"
#include "net/SocketUtil.h"
#include "net/Socket.h"

using namespace std;
using namespace tigerso::core;
using namespace tigerso::net;
using namespace tigerso::http;

int create_socket(Socket& server, const char* host)
{
    vector<string> ipv;
    int ret = SocketUtil::ResolveHost2IP(string(host), ipv);
    if(ret != 0) {
        printf("DNS resolve failed!\n");
        return -1;
    }

    return SocketUtil::Connect(server, ipv[0], "443");
}

void init_openssl()
{ 
    SSL_load_error_strings();   
    OpenSSL_add_ssl_algorithms();
}

void cleanup_openssl()
{
    EVP_cleanup();
}

SSL_CTX *create_context()
{
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = SSLv23_client_method();

    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx;
}

void configure_context(SSL_CTX *ctx)
{
    SSL_CTX_set_ecdh_auto(ctx, 1);

    /* Set the key and cert */
    if (SSL_CTX_load_verify_locations(ctx, NULL, "../ssl/cert/trustCA") <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

}

int main(int argc, char **argv)
{
    if(argc < 2) {
        return -1;
    }

    const char* host = argv[1];
    Socket sock;
    SSL_CTX *ctx;

    init_openssl();
    ctx = create_context();

    configure_context(ctx);

    create_socket(sock, host);
    sock.setNIO(false);

    SSL* ssl = SSL_new(ctx);
    SSL_set_tlsext_host_name(ssl, host);

    SSL_set_fd(ssl, sock.getSocket());

    int ret = SSL_connect(ssl);
    if(ret != 1) {
        int error = SSL_get_error(ssl, ret);
        ERR_print_errors_fp(stderr);
        return error;
    }
    
    HttpRequest request;
    request.setMethod("GET");
    request.setUrl("/");
    char hostvalue[1024] = {0};
    sprintf(hostvalue, "%s:%d", host, 443);
    request.setValueByHeader("Host", hostvalue);
    string req = request.toString();
    
    SSL_write(ssl, req.c_str(), req.size());
    char response[1024] = {0};
    SSL_read(ssl, response, 1024);
    printf("response: %s\n", response);

    cleanup_openssl();

    return 0;

}
