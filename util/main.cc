#include "util/FileTypeDetector.h"

using namespace tigerso;

int main(int argc, char* argv[]) {
    
    if(argc < 2) {
        return 0;
    }

    const char* filename = argv[1];
    FileTypeDetector type;
    const char* classnane = type.detectFile(filename);
    printf("File:%s, type:%s\n", filename, classnane);
    return 0;

}
