#include "core/Dechex.h"

namespace tigerso {

static char hexstrbuf[9] = {0};
void REVERSE_STRING(char* beg, char* end) {
    do {
        char tmp;
        while(beg < end) {
           tmp = *beg;
           *beg = *end;
           *end = tmp;
           beg++;
           end--;
        }
    } while(0);
    return;
}

const char* dec2hex(unsigned int num) {
    unsigned int dec = num;
    bzero(hexstrbuf,sizeof(hexstrbuf));
    while(dec) {
        PUSH2HEXBUF(HEX2CHAR(HEX_MOD(dec)));
        dec = HEX_DIV(dec);
    }
    
    REVERSESTRING(hexstrbuf);
    return hexstrbuf;
}

unsigned int hex2dec(const char* hex) {
    const char* rbeg = hex + strlen(hex) - 1;
    const char* rend = hex;

    unsigned int dec = 0;
    int i = 0;
    do{
        dec += (HEXCHAR2DEC(*rbeg) << (4*i));
        i++;
        rbeg--;
    }while(rbeg >= rend);
    return dec;
}
}//namespace tigerso
