#include <stdlib.h>
#include <string.h>
#include <string>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "core/SysUtil.h"
#include "net/SocketUtil.h"
#include "net/Socket.h"
#include "ssl/SSLContext.h"
#include "core/ConfigParser.h"

using namespace std;
using namespace tigerso;

static ConfigParser* g_config = ConfigParser::getInstance();
static Logging* g_log = Logging::getInstance();

void config() {
    if(g_config != NULL) {
        g_config->setConfigPath("../etc/tigerso.ini");
    }
    if(g_log!=NULL) {
        g_log->setLogPath("../test/log");
    }
}

int create_socket(int port)
{
    int s;
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("Unable to create socket");
        exit(EXIT_FAILURE);
    }

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Unable to bind");
        exit(EXIT_FAILURE);
    }

    if (listen(s, 1) < 0) {
        perror("Unable to listen");
        exit(EXIT_FAILURE);
    }

    return s;
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



int main(int argc, char **argv)
{
    config();
    Socket listenSocket;
    SocketUtil::CreateListenSocket("10.64.75.131", "443", true, listenSocket);

    /* Handle connections */
    while(1) {
        const char reply[] = "<html><head>hahahaaaas</head></html>\n";

        Socket client;
        while(!client.exist()) {
            SocketUtil::Accept(listenSocket, client);
            perror("Unable to accept");
            //exit(EXIT_FAILURE);
        }

        SSLContext sctx;
        sctx.init(SCTX_ROLE_SERVER);
        sctx.bindSocket(client.getSocket());

        if (sctx.accept() != SCTX_IO_OK) {
            ERR_print_errors_fp(stderr);
        }
        else {
            char buffer[10240] = {0};
            size_t ion = 0;
            int ret = sctx.recv(buffer, 10240, &ion);
            printf("return: %d, recv %d bytes, request:\n%s\n", ret, ion, buffer);
            ret = sctx.send(reply, strlen(reply), &ion);
            printf("return: %d, send %d bytes",ret, ion);
        }

        sctx.close();
        client.close();
    }

    listenSocket.close();
    cleanup_openssl();
}

