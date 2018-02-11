#ifndef TS_CORE_HASH_H_
#define TS_CORE_HASH_H_

namespace tigerso {

typedef unsigned long HashKey;
typedef HashKey HashSize;

namespace Hash {

HashKey hash(const char* keyname, const HashSize& hashsize);
HashSize adjustHashSize(const HashSize&);

} //namespace Hash


} //namespace tigerso

#endif
