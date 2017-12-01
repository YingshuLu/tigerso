#include <iostream>
#include <string>
#include <string.h>

using namespace std;

int main(int argc, char* argv[]) {
    if(argc < 2) {
        return 0;
    }

    const char* host = argv[1];
    size_t len = strlen(host);
    cout << "1. Host: " << host << ", len: " << len << endl;

    string query_name = string(host, len);
    
    char domain[255] = {0};
    memcpy(domain, host, len);
    
    cout << "2. Domain: " << domain << ", len: " << strlen(domain) << endl;

    return 0;
}
