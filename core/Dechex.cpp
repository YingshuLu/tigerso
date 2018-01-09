#include "core/Dechex.h"

namespace tigerso {

static const char HexCharSet[] = {
'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

static char hexstrbuf[9] = {0};

#define HEX_MOD(num) (num & 0xF)
#define HEX_DIV(num) (num >> 4)
#define HEXCHAR2DEC(hexchar) (hexchar > '9'? hexchar - 'a'+ 10 : hexchar - '0')
#define HEX2CHAR(hex) (HexCharSet[hex])
#define PUSH2HEXBUF(ch) (hexstrbuf[strlen(hexstrbuf)] = ch)
#define REVERSESTRING(str) (REVERSE_STRING(str, str+strlen(str)-1))

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
