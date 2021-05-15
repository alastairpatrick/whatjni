#ifndef WHATJNI_WEAK_REF_H
#define WHATJNI_WEAK_REF_H

#include "whatjni/ref.h"

#include <unordered_set>

namespace whatjni {

// Wraps a JNI WeakGlobalRef. Except for some specific exceptions, generally T may be an incomplete type, i.e. only
// forward declared.
template <typename T>
class weak_ref {
    template <typename U> friend class weak_ref;
    jobject obj = nullptr;
public:
    weak_ref() {}
    weak_ref(std::nullptr_t) {}

    weak_ref(T* rhs) {
        obj = new_weak_global_ref((jobject) rhs);
    }
    template <typename U> weak_ref(U* rhs) {
        T* t = (U*) nullptr;  // static assertion
        obj = new_weak_global_ref((jobject) rhs);
    }

    weak_ref(const weak_ref& rhs) {
        obj = new_weak_global_ref(rhs.obj);
    }
    template<typename U> weak_ref(const weak_ref<U>& rhs) {
        T* t = (U*) nullptr;  // static assertion
        obj = new_weak_global_ref(rhs.obj);
    }

    weak_ref(weak_ref&& rhs) {
        obj = rhs.obj;
        rhs.obj = nullptr;
    }
    template<typename U> weak_ref(weak_ref<U>&& rhs) {
        T* t = (U*) nullptr;  // static assertion
        obj = rhs.obj;
        rhs.obj = nullptr;
    }

    ~weak_ref() {
        delete_weak_global_ref(obj);
    }

    weak_ref& operator=(std::nullptr_t) {
        delete_weak_global_ref(obj);
        obj = nullptr;
        return *this;
    }
    weak_ref& operator=(const weak_ref& rhs) {
        if (this != &rhs) {
            delete_weak_global_ref(obj);
            obj = new_weak_global_ref(rhs.obj);
        }
        return *this;
    }
    template<typename U> weak_ref& operator=(const weak_ref<U>& rhs) {
        T* t = (U*) nullptr;  // static assertion
        delete_weak_global_ref(obj);
        obj = new_weak_global_ref(rhs.obj);
        return *this;
    }

    weak_ref& operator=(weak_ref&& rhs) {
        if (this != &rhs) {
            delete_weak_global_ref(obj);
            obj = rhs.obj;
            rhs.obj = nullptr;
        }
        return *this;
    }
    template<typename U> weak_ref& operator=(weak_ref<U>&& rhs) {
        T* t = (U*) nullptr;  // static assertion
        delete_weak_global_ref(obj);
        obj = rhs.obj;
        rhs.obj = nullptr;
        return *this;
    }

    T* lock() const {
        return (T*) new_local_ref(obj);
    }
};

}  // namespace whatjni

#endif  // WHATJNI_WEAK_REF_H
