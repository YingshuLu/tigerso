#ifndef TS_CORE_BASECLASS_H_
#define TS_CORE_BASECLASS_H_

//Base class

#include <memory>

namespace tigerso {

class nocopyable {
public:
    nocopyable(){}
    virtual ~nocopyable(){}
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

template <typename T> class Singleton {
public:
    static T* getInstance() {
        if(!pInstance_.get()) { pInstance_ = std::unique_ptr<T>(new T()); }
        return pInstance_.get();
    }

    virtual ~Singleton() {}
protected:
    Singleton() {}

private:
    static std::unique_ptr<T> pInstance_;
};

template <typename T>
std::unique_ptr<T> Singleton<T>::pInstance_;

} //namespace ts
#endif // TS_CORE_BASECLASS_H_
