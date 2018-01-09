#ifndef TS_CORE_DECHEX_H_
#define TS_CORE_DECHEX_H_

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

namespace tigerso {
    
const char* dec2hex(unsigned int num);
unsigned int hex2dec(const char* hex);

}//namespace tigerso

#endif
