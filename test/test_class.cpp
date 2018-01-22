#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory>
#include <functional>

class parent {

public:
    parent() {}
    int process() {
        return vp();
    }

    virtual int vp() {
        printf("parent vp\n");
        return 0;
    }

    virtual ~parent() {}
};

class child: public parent{

public:
    child() {}
    int vp() {
        printf("child vp\n");
        return 0;
    }
    ~child() {}
};

int main() {
    parent* p = new child;
    auto func = std::bind(&parent::process, p);
    func();

    parent* c = new parent;
    auto funp = std::bind(&parent::process, c);
    funp();
    delete p;
    delete c;
    return 0;
}
