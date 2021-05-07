#ifndef WHATJNI_REF_H
#define WHATJNI_REF_H

#include "whatjni/base.h"

#include <unordered_set>

namespace java {
namespace lang {
class String;
}  // namespace lang
}  // namespace java

namespace whatjni {

static const struct {} own_ref;

// T* is actually a JNI LocalRef or GlobalRef, selected automatically depending on whether a given ref<T> is resides on
// the stack or not. Except for some specific exceptions, generally T may be an incomplete type, i.e. only forward
// declared.
template <typename T>
class ref {
    template <typename U> friend class ref;
    T* obj = nullptr;
public:
    typedef T Class;

    ref() {}
    ref(std::nullptr_t) {}

    ref(jobject rhs, decltype(own_ref)) {
        obj = (T*) rhs;
    }
    ref(T* rhs, decltype(own_ref)) {
        obj = rhs;
    }

    ref(T* rhs) {
        new_auto_ref(&(jobject) obj, (jobject) rhs);
    }
    template <typename U>ref(U* rhs) {
        T* t = (U*) nullptr;  // static assertion
        new_auto_ref(&(jobject) obj, (jobject) rhs);
    }

    ref(const ref& rhs) {
        new_auto_ref(&(jobject) obj, (jobject) rhs.obj);
    }
    template<typename U> ref(const ref<U>& rhs) {
        T* t = (U*) nullptr;  // static assertion
        new_auto_ref(&(jobject) obj, (jobject) rhs.obj);
    }

    ref(const char16_t* str) {
        T* t = (::java::lang::String*) nullptr;  // static assert
        auto length = std::char_traits<char16_t>::length(str);
        jobject local = new_string((const jchar*) str, length);
        move_auto_ref(&(jobject) obj, &local);
    }
    ref(const std::u16string& str) {
        T* t = (::java::lang::String*) nullptr;  // static assert
        jobject local = new_string((const jchar*) str.data(), str.length());
        move_auto_ref(&(jobject) obj, &local);
    }

#if WHATJNI_LANG >= 201703L
    ref(std::u16string_view str) {
        T* t = (::java::lang::String*) nullptr;  // static assert
        jobject local = new_string((const jchar*) str.data(), str.length());
        move_auto_ref(&(jobject) obj, &local);
    }
#endif

    ref(ref&& rhs) {
        move_auto_ref(&(jobject) obj, &(jobject) rhs.obj);
    }
    template<typename U> ref(ref<U>&& rhs) {
        T* t = (U*) nullptr;  // static assertion
        move_auto_ref(&(jobject) obj, &(jobject) rhs.obj);
    }

    ~ref() {
        delete_auto_ref(&(jobject) obj);
    }

    ref& operator=(std::nullptr_t) {
        delete_auto_ref(&(jobject) obj);
        obj = nullptr;
        return *this;
    }
    ref& operator=(const ref& rhs) {
        if (this != &rhs) {
            delete_auto_ref(&(jobject) obj);
            new_auto_ref(&(jobject) obj, (jobject) rhs.obj);
        }
        return *this;
    }
    template<typename U> ref& operator=(const ref<U>& rhs) {
        T* t = (U*) nullptr;  // static assertion
        delete_auto_ref(&(jobject) obj);
        new_auto_ref(&(jobject) obj, (jobject) rhs.obj);
        return *this;
    }

    ref& operator=(ref&& rhs) {
        if (this != &rhs) {
            delete_auto_ref(&(jobject) obj);
            move_auto_ref(&(jobject) obj, &(jobject) rhs.obj);
        }
        return *this;
    }
    template<typename U> ref& operator=(ref<U>&& rhs) {
        T* t = (U*) nullptr;  // static assertion
        delete_auto_ref(&(jobject) obj);
        move_auto_ref(&(jobject) obj, &(jobject) rhs.obj);
        return *this;
    }

    T* operator->() const {
        return obj;
    }
    operator bool() const {
        return obj;
    }

    template <typename U> bool operator==(const ref<U>& rhs) const {
        return is_same_object((jobject) obj, (jobject) rhs.obj);
    }
    template <typename U> bool operator!=(const ref<U>& rhs) const {
        return !is_same_object((jobject) obj, (jobject) rhs.obj);
    }
};

template <typename T> bool operator==(const ref<T>& lhs, std::nullptr_t) {
    return !lhs;
}
template <typename T> bool operator!=(const ref<T>& lhs, std::nullptr_t) {
    return lhs;
}
template <typename T> bool operator==(std::nullptr_t, const ref<T>& rhs) {
    return !rhs;
}
template <typename T> bool operator!=(std::nullptr_t, const ref<T>& rhs) {
    return rhs;
}

template<typename T> struct by_value {
    std::size_t operator()(const ref<T>& r) const {
        return (std::size_t) get_hash_code((jobject) r.operator->());
    }
    bool operator()(const ref<T>& lhs, const ref<T>& rhs) const {
        return is_equal_object((jobject) lhs.operator->(), (jobject) rhs.operator->());
    }
};

template<typename T> struct by_identity {
    std::size_t operator()(const ref<T>& r) const {
        return (std::size_t) get_identity_hash_code((jobject) r.operator->());
    }
    bool operator()(const ref<T>& lhs, const ref<T>& rhs) const {
        return lhs == rhs;
    }
};

}  // namespace whatjni

namespace std {

template<typename T> struct hash<::whatjni::ref<T>> {
    // must not be by_value<T>. by_identity<T> for consistency with std::equal_to<ref<T>>.
    ::whatjni::by_identity<T> hasher;
    std::size_t operator()(const ::whatjni::ref<T>& r) const {
        return hasher(r);
    }
};

}  // namespace std

#endif  // WHATJNI_REF_H
