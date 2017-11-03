#include "DNSResolver.h"
#include <iostream>

using namespace std;
using namespace tigerso::dns;

int main(int argc, char* argv[]) {

    if(argc < 2) {
        return 0;
    }

    DNSResolver resolver;
    string domain = argv[1];
    resolver.queryDNS(domain);

    string ip;
    time_t ttl;
    resolver.getAnswer(ip, ttl);
    cout <<">> Test My DNSResolver <<\n Question: " << domain << "\n Answer:" << ip << "\n TTL: " << ttl << endl;
    return 0;


    
}
