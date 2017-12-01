#include <iostream>

using namespace std;
class mytest;
class peer {
public:
    peer() {}
    mytest* test;
};

class mytest {
public:
    mytest(){}
    int info() { cout << "haha, not see me" << endl;}
   int  deleteself() {
        delete pe->test; 
        cout << "no instance , but I in its function" << endl;
        cout << "this: " << this <<endl;
        info();
        return 0;
    }

   ~mytest() {
    cout << "delete myself" <<endl;
   }
    peer* pe;
    static int testdata;
    static int testdata2;
};

int main() {
    mytest* tp = new mytest();

    peer pee;
    pee.test = tp;

    tp->pe = &pee;
    tp->deleteself();
    cout << pee.test << endl;
    cout << "Done" <<endl;
    
    cout << "sizeof peer:" << sizeof(pee) << endl;
    cout << "sizeof mytest: " << sizeof(*tp) <<endl;
    cout << "mytest::info : " << &mytest::info <<endl; 
    cout << "mytest::deleteself : " << &mytest::deleteself <<endl; 
    return 0;
}
