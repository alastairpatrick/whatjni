#ifndef WHATJNI_TYPE_TRAITS_H
#define WHATJNI_TYPE_TRAITS_H

#include "whatjni/base.h"

namespace whatjni {

template <typename T> class array;

// T does not correspond to a Java type. Did you forget append pointer type with '*'?
template <typename T>
struct traits {
};

// traits for all types that are Java classes, i.e. those types that are not Java primitive types. Specialized later
// for each of the eight primitive types.
template <typename T>
struct traits<T*> {
    // How this type is communicated with the low level base API.
    typedef jobject BaseType;
    static T* from_base(BaseType value) {
        return (T*) value;
    }
    static BaseType to_base(T* value) {
        return (jobject) value;
    }

    // Create an array of this type of the given length.
    static array<T*>* new_array(jsize size) {
        return (array<T*>*) new_object_array(size, traits<T*>::get_class(), nullptr);
    }

    // JVM signature for this type, "Ljava/lang/String;" style for classes.
    static std::string get_signature() {
        return T::get_signature();
    }

    // Class for this type. Only present for types that are classes, i.e. not for primitive types.
    static jclass get_class() {
        static jclass clazz = find_class(signature_to_class_name(get_signature()).c_str());
        return clazz;
    }

private:
    static std::string signature_to_class_name(const std::string& sig) {
        if (sig.length() >= 2 && sig[0] == 'L' && sig[sig.length() - 1] == ';') {
            return sig.substr(1, sig.length() - 2);
        }
        return sig;
    }
};

template <typename T>
struct primitive_traits {
    typedef T BaseType;

    static array<T>* new_array(jsize size) {
        return (array<T>*) new_primitive_array<T>(size);
    }

    static T from_base(BaseType value) {
        return value;
    }

    static BaseType to_base(T value) {
        return value;
    }
};

template <>
struct traits<jboolean> : primitive_traits<jboolean> {
    static std::string get_signature() {
        return "Z";
    }
};

template <>
struct traits<jbyte> : primitive_traits<jbyte> {
    static std::string get_signature() {
        return "B";
    }
};

template <>
struct traits<jshort> : primitive_traits<jshort> {
    static std::string get_signature() {
        return "S";
    }
};

template <>
struct traits<jint> : primitive_traits<jint> {
    static std::string get_signature() {
        return "I";
    }
};

template <>
struct traits<jlong> : primitive_traits<jlong> {
    static std::string get_signature() {
        return "J";
    }
};

template <>
struct traits<jchar> : primitive_traits<jchar> {
    static std::string get_signature() {
        return "C";
    }
};

template <>
struct traits<jfloat> : primitive_traits<jfloat> {
    static std::string get_signature() {
        return "F";
    }
};

template <>
struct traits<jdouble> : primitive_traits<jdouble> {
    static std::string get_signature() {
        return "D";
    }
};

}  // namespace whatjni

#endif  // WHATJNI_TYPE_TRAITS_H
