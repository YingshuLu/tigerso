#ifndef TS_CORE_BASECLASS_H_
#define TS_CORE_BASECLASS_H_

//Base class

namespace tigerso::core {

class nocopyable {
public:
    nocopyable(){}
private:
    nocopyable(const nocopyable&);
    nocopyable& operator=(const nocopyable&);
};

class singleton {
public:
    virtual singleton* getInstance() = 0;
    virtual ~singleton(){ if ( pInstance != nullptr) { delete pInstance; } };
protected:
    static singleton* pInstance; 
    singleton();
    singleton(const singleton&);
    singleton& operator=(const singleton&);
};


} //namespace tscore
#endif // TS_CORE_BASECLASS_H_
