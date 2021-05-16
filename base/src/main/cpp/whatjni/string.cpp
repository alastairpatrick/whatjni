#include "whatjni/string.h"

namespace whatjni {

std::string WHATJNI_BASE std_string(java::lang::String* str) {
    jstring jstr = reinterpret_cast<jstring>(str);
    size_t length = get_string_multibyte_size(jstr);

#if WHATJNI_LANG >= 201703L
    std::string result(length, 0);
    get_string_region(jstr, 0, length, result.data());
#else
    std::string result;
    jboolean is_copy;
    const char* chars = get_string_multibyte_chars(jstr, &is_copy);
    result.append(chars, length);
    release_string_multibyte_chars(jstr, chars);
#endif

    return result;
}

std::u16string WHATJNI_BASE std_u16string(java::lang::String* str) {
    jstring jstr = reinterpret_cast<jstring>(str);
    size_t length = get_string_length(jstr);

#if WHATJNI_LANG >= 201703L
    std::u16string result(length, 0);
    get_string_region(jstr, 0, length, result.data());
#else
    std::u16string result;
    jboolean is_copy;
    const char16_t* chars = get_string_utf16_chars_critical(jstr, &is_copy);
    result.append(chars, length);
    release_string_utf16_chars_critical(jstr, chars);
#endif

    return result;
}

}  // namespace whatjni

std::ostream& operator<<(std::ostream& os, java::lang::Object* obj) {
    using namespace whatjni;

    if (!obj) {
        os << "null";
        return os;
    }

    jstring jstr = to_string(reinterpret_cast<jobject>(obj));

    if (!jstr) {
        os << "null";
        return os;
    }

    size_t length = get_string_multibyte_size(jstr);

    jboolean is_copy;
    const char* chars = get_string_multibyte_chars(jstr, &is_copy);
    os.write(chars, length);
    release_string_multibyte_chars(jstr, chars);

    return os;
}

std::ostream& operator<<(std::ostream& os, java::lang::String* obj) {
    return os << reinterpret_cast<java::lang::Object*>(obj);
}
