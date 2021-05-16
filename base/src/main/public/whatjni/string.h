#ifndef WHATJNI_STRING_H
#define WHATJNI_STRING_H

#include "whatjni/base.h"

#include <cassert>
#include <iostream>
#include <string>

namespace java {
namespace lang {
class Object;
class String;
}  // namespace lang
}  // namespae java

namespace whatjni {

// JNI proprietary multibyte encoding. This is generally not UTF-8. It uses the same representation as UTF-8 for strings
// that do not contain null characters or characters outside the Basic Multilingual Plane. Otherwise it is different
// from UTF-8. See the section "Modified UTF-8 Strings" in the JNI specification.
//
// https://docs.oracle.com/javase/7/docs/technotes/guides/jni/spec/types.html#wp16542
//
// We use std::string here because it doesn't imply any particular character encoding. If we add support for C++20
// std::u8string, it will be for true UTF-8 encoding.
//
// Note that null characters are encoded as the two byte sequence [0xC0, 0x80] so there are never any internal null
// bytes.
WHATJNI_BASE std::string std_string(java::lang::String* str);

// Actual UTF-16 with no deviation from the standard.
WHATJNI_BASE std::u16string std_u16string(java::lang::String* str);

inline java::lang::String* j_string(const char* str) {
    return (java::lang::String*) new_string(str);
}

inline java::lang::String* j_string(const char16_t* str) {
    return (java::lang::String*) new_string(str, std::char_traits<char16_t>::length(str));
}

inline java::lang::String* j_string(const char16_t* str, size_t length) {
    return (java::lang::String*) new_string(str, length);
}

inline java::lang::String* j_string(const std::string& str) {
    // There should be no internal null bytes; JNI multibyte encoding represents null characters with the two byte
    // sequence [0xC0, 0x80].
    assert(str.length() == strlen(str.c_str()));
    return j_string(str.c_str());
}

inline java::lang::String* j_string(const std::u16string& str) {
    return j_string(str.data(), str.length());
}

inline java::lang::String* operator ""_j(const char* str, std::size_t length) {
    // There should be no internal null bytes; JNI multibyte encoding represents null characters with the two byte
    // sequence [0xC0, 0x80].
    assert(length == strlen(str));
    return j_string(str);
}

}  // namespace whatjni

WHATJNI_BASE std::ostream& operator<<(std::ostream& os, java::lang::Object* obj);

// Redundant if String is a complete type.
WHATJNI_BASE std::ostream& operator<<(std::ostream& os, java::lang::String* obj);

#endif  // WHATJNI_STRING_H
