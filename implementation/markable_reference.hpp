#pragma once

#include <atomic>
#include <thread>

namespace pointer {
    //https://stackoverflow.com/questions/40247249/what-is-the-c-11-atomic-library-equivalent-of-javas-atomicmarkablereferencet
    template<class T>
    class MarkableReference
    {
    private:
        uintptr_t val;
        static const uintptr_t mask = 1;
    public:
        MarkableReference(T* ref = NULL, bool mark = false) {
            val = ((uintptr_t)ref & ~mask) | (mark ? 1 : 0);
        }
        T* getRef() const { return (T*)(val & ~mask); }
        bool getMark() const { return (val & mask); }
        T *operator->() const { return (T*)(val & ~mask); }
    };
}