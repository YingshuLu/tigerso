#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include "HttpBodyFile.h"

#define nonBlocking(fd) fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK)

#define SEND_DEBUG
#ifndef SEND_DEBUG
#define DBG_LOG //printf
#else
#define DBG_LOG printf
#endif

#define showComplete() showProcess(100, 100)

void showProcess(long now, long total) {

    int i = 0;
    printf("\r");

    float fper = now*100 / total;
    int percent = (int) fper;

    for(; i < percent; i++) {
        printf("=");
    }

    printf(">>[%d%%]", percent);

    fflush(stdout);
}

ssize_t getFileLength(int fd) {
    struct stat st;
    if(fstat(fd, &st) != 0) {
        return 0;
    }
    
    return ssize_t(st.st_size);
}

int listenfd = socket(AF_INET, SOCK_STREAM, 0);

// need more error processes...
int listeningOnLocal() {
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(76);
    inet_pton(AF_INET, "10.64.75.131", &addr.sin_addr);

    bind(listenfd, (sockaddr*)&addr, sizeof(addr));
    listen(listenfd, 16);
    int new_fd = dup2(listenfd, 253);
    close(listenfd);
    listenfd = new_fd;

    return listenfd;
}

ssize_t sendFile(int out_fd, int in_fd, ssize_t len) {
    
    off_t off = 0;
    ssize_t n = 0;
    ssize_t left = len;

    while (left > 0 && (n = sendfile(out_fd, in_fd, &off, left)) < left) {
        if( 0 >= n ) {
            if( 0 == n) {
                DBG_LOG("sendfile return 0, errno: %d, err: %s\n", errno, strerror(errno));
                return 0;
            }
            else if (errno == EAGAIN) {
                continue;
            }
            else {
                DBG_LOG("sendfile return -1, errno: %d, err: %s\n", errno, strerror(errno));
                return 0;
            }
        }
        //DBG_LOG("off: %ld, left: %ld\n", off, left);
        //DBG_LOG("%ld bytes copied\n", n);
        showProcess(off, len-1);
        left -= n;
    }

    return len;
}


int main(int argc, char* argv[]) {
    if(argc < 2) {
        return 0;
    }

    const char* infile = argv[1];
    HttpBodyFile file(infile);

    //daemon(0,0);
    ssize_t file_len = file.size();
    DBG_LOG("file: %s, size: %ld \n", infile, file_len);
    if(file_len == 0) {
        return -1;
    }

    int out_fd;
    file.setFile(infile);
    int listenfd = listeningOnLocal();
    while((out_fd = accept(listenfd, NULL, NULL)) >= 0) {
        printf("accept connection, socket: %d\n", out_fd);
        if(out_fd < 0 || nonBlocking(out_fd) == -1) {
            DBG_LOG("nonBlocking fd: %d error. %s\n", out_fd, strerror(errno));
            return -1;
        }
        char buffer[63336] = {0};
        int readn = read(out_fd, buffer, 65536);
        printf("recv %d bytes\n", readn);
        RingBuffer rb;
        File tf("./get.log");
        rb.writeIn(buffer, readn);
        rb.readOut2File(tf);

        time_t now = time(NULL);
        const char* header = "HTTP/1.1 200 OK\r\nServer: squid/3.5.24\r\nDate: Tue, 12 Dec 2017 13:17:22 GMT\r\nContent-Type: text/html; charset=GB2312\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n";
        RingBuffer ringbuf;
        ringbuf.writeIn(header, strlen(header));
        int ret = 0;

        do {
            ret = ringbuf.send2Socket(out_fd);
            printf("send %d bytes\n", ret);
            if(ret < 0) {
                ::close(out_fd);
            }
        }
        while(ringbuf.size());

        int res = 1;
        size_t total = 0;

        do{
            size_t sendn = 0;
            res = file.sendChunk2Socket(out_fd);
            //DBG_LOG("file send2socket return: %d, send %d bytes\n", res, (int)sendn);
            //total += sendn;
            //showProcess(total, file_len);
        }
        while(res == FILE_SENDFILE_RECALL);

        time_t cost = time(NULL) - now;

        showComplete();
        DBG_LOG("\ntransport cost:%ld seconds\n", cost);
        printf("\tTransfer completed!\n");
        DBG_LOG("now close client fd: %d\n", out_fd);
        shutdown(out_fd, SHUT_RDWR);
    }

    printf("accept errno: %d: %s\n", errno, strerror(errno));
    close(listenfd);
    return 0;
}
