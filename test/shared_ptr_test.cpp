#include <stdio.h>
#include <string.h>
#include <functional>
#include <memory>

class Test: public std::enable_shared_from_this<Test> {

public:
    Test() {}
    void myfunc() {
       printf("test my function"); 
    }

};

typedef std::function<void()> FUNC;

int main() {

    std::shared_ptr<Test> testptr;

    FUNC func = std::bind(&Test::myfunc, testptr);
    func();
    return 0;
}
