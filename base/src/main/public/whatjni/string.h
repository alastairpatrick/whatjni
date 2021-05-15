#ifndef WHATJNI_STRING_H
#define WHATJNI_STRING_H

#include "whatjni/base.h"

#include <string>

namespace whatjni {

inline java::lang::String* j_string(const char* str, std::size_t length) {
    return (java::lang::String*) new_utf_string(str, length);
}

inline java::lang::String* j_string(const char* str) {
    return (java::lang::String*) new_utf_string(str);
}

inline java::lang::String* jstring(const std::string& str) {
    return j_string(str.data(), str.length());
}

#if WHATJNI_LANG >= 201703L
inline java::lang::String* j_string(const std::string_view& str) {
    return j_string(str.data(), str.length());
}
#endif

inline java::lang::String* operator ""_j(const char* str, std::size_t length) {
    return j_string(str, length);
}

}  // namespace whatjni

#endif  // WHATJNI_STRING_H
