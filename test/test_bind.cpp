#include <functional>
#include <iostream>
#include <string>

using namespace std;

class nocopy {
public:
    nocopy(){}
    int& getID() {return id;}
private:
    nocopy(const nocopy&);
    nocopy& operator=(const nocopy&);

private:
  int id = 0;  
};

class testbind {

public:
    testbind() {}

    int test(nocopy& np) {
       return np.getID();
    }

private:
    testbind(const testbind&);
    testbind& operator=(const testbind&);

};

typedef std::function<int(nocopy&)> nocopy_func;

int print_class(nocopy_func func, nocopy& copy) {
    if(func == nullptr){ return -1; }
    cout << "bind nocopy: "<< func(copy) << endl;
    return 0;
}

int testf(function<int()> func) {
   return func(); 
}

int main() {

    testbind tb;
    nocopy np;
    int& id = np.getID();
    id += 4;

    print_class(bind(&testbind::test, &tb, placeholders::_1), np);
    testbind(bind(&testbind::test, &tb, np));
    return 0;
}
