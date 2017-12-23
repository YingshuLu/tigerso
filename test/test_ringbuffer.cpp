#include <stdio.h>
#include "RingBuffer.h"

char buffer[] = "I am testing RingBuffer cache!";
char buffer2[] = "padding to fulfill the cache! hahaha";
/*
void printchar(char* buf, size_t len) {
    size_t i = 0;
    while(i < len) {
        printf("%c", *(buf + i));
        i++;
    }
    printf("\n");
}
*/
int main() {
    RingBuffer rbuf(64);
    int ret = rbuf.writeIn(buffer, sizeof(buffer)-1);
    printf("RingBuffer size(): %ld, return %d\n", rbuf.size(), ret);
    if(rbuf.isFull()) { 
        printf("RingBuffer is Full\n");
    }
    ret = rbuf.writeIn(buffer2, sizeof(buffer2)-1);
    printf("RingBuffer size(): %ld, return %d\n", rbuf.size(), ret);
    if(rbuf.isFull()) { 
        printf("RingBuffer is Full\n");
    }
 
    char exten[12] = {2};
    ret = rbuf.writeIn(exten, 12);
    printf("RingBuffer size(): %ld, return %d\n", rbuf.size(), ret);

    ret = rbuf.readOut(exten, 12);
    printf("RingBuffer size(): %ld, return %d\n", rbuf.size(), ret);
    
    printchar(exten, 12);

    ret = rbuf.readOut(exten, 12);
    printf("RingBuffer size(): %ld, return %d\n", rbuf.size(), ret);

    printchar(exten, 12);

    ret = rbuf.readOut(exten, 12);

    printf("RingBuffer size(): %ld, return %d\n", rbuf.size(), ret);

    printchar(exten, 12);

    char buf[] = "haha eee asdasdas asdfsdfsdfsda asdasdasd";
    ret = rbuf.writeIn(buf, sizeof(buf)-1);
    printf("RingBuffer size(): %ld, return %d\n", rbuf.size(), ret);
    if(rbuf.isFull()) { 
        printf("RingBuffer is Full\n");
    }

    char readbuf[64] = {0};
    ret = rbuf.readOut(readbuf, 64);
 

    printf("RingBuffer size(): %ld, return %d\n", rbuf.size(), ret);
    if(rbuf.isEmpty()) {
        printf("RingBuffer is Empty\n");
    }

    return 0;
}
