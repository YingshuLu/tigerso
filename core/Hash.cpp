#include "string.h"
#include "stdlib.h"
#include "core/Hash.h"

namespace tigerso {

namespace Hash {

//From STL 
static const unsigned long _HASH_PRIMES[] =
{
      53ul,         97ul,         193ul,       389ul,       769ul,
      1543ul,       3079ul,       6151ul,      12289ul,     24593ul,
      49157ul,      98317ul,      196613ul,    393241ul,    786433ul,
      1572869ul,    3145739ul,    6291469ul,   12582917ul,  25165843ul,
      50331653ul,   100663319ul,  201326611ul, 402653189ul, 805306457ul,
      1610612741ul, 3221225473ul, 4294967291ul
};

//From nginx & JDK hash
HashKey hash(const char* keyname, const HashSize& hashsize) {
    HashKey key = 0;
    int len = strlen(keyname);
    for(int i = 0; i <= len; i++)
        key = key * 31 + keyname[i];
    return (key % hashsize);
}

HashSize adjustHashSize(const HashSize& size) {
    int i = 0;
    for(; i < sizeof(_HASH_PRIMES)/sizeof(unsigned long); i++) {
        if(size < _HASH_PRIMES[i]) 
            return _HASH_PRIMES[i];
    }
    return _HASH_PRIMES[i - 1];
}



}//namespace Hash

}//namespace tigerso
