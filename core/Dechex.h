#ifndef TS_CORE_DECHEX_H_
#define TS_CORE_DECHEX_H_

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

namespace tigerso {

static const char HexCharSet[] = {
'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

#define HEX_MOD(num) (num & 0xF)
#define HEX_DIV(num) (num >> 4)
#define HEXCHAR2DEC(hexchar) (hexchar > '9'? hexchar - 'a'+ 10 : hexchar - '0')
#define HEX2CHAR(hex) (HexCharSet[hex])
#define PUSH2HEXBUF(ch) (hexstrbuf[strlen(hexstrbuf)] = ch)
#define REVERSESTRING(str) (REVERSE_STRING(str, str+strlen(str)-1))

void REVERSE_STRING(char* beg, char* end);
const char* dec2hex(unsigned int num);
unsigned int hex2dec(const char* hex);

}//namespace tigerso

#endif
