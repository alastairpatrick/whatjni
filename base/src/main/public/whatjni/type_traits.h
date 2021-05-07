#ifndef WHATJNI_TYPE_TRAITS_H
#define WHATJNI_TYPE_TRAITS_H

namespace whatjni {

template <typename T> class array;
template <typename T> class ref;

// TypeTraits for all types that are Java classes, i.e. those types that are not Java primitive types. Specialized later
// for each of the eight primitive types.
template <typename T>
struct TypeTraits {
    // How this type is communicated with the low level base API.
    typedef jobject BaseType;
    static T from_base(BaseType value) {
        return T(value, own_ref);
    }
    static BaseType to_base(const T& value) {
        return (jobject) value.operator->();
    }

    // Create an array of this type of the given length.
    static ref<array<T>> new_array(jsize size) {
        return ref<array<T>>(new_object_array(size, TypeTraits<T>::get_class(), nullptr), own_ref);
    }

    // JVM signature for this type, "Ljava/lang/String;" style for classes.
    static std::string get_signature() {
        typedef typename T::Class Class;
        return Class::get_signature();
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
struct PrimitiveTypeTraits {
    typedef T BaseType;

    static ref<array<T>> new_array(jsize size) {
        return ref<array<T>>(new_primitive_array<T>(size), own_ref);
    }

    static T from_base(BaseType value) {
        return value;
    }

    static BaseType to_base(T value) {
        return value;
    }
};

template <>
struct TypeTraits<jboolean> : PrimitiveTypeTraits<jboolean> {
    static std::string get_signature() {
        return "Z";
    }
};

template <>
struct TypeTraits<jbyte> : PrimitiveTypeTraits<jbyte> {
    static std::string get_signature() {
        return "B";
    }
};

template <>
struct TypeTraits<jshort> : PrimitiveTypeTraits<jshort> {
    static std::string get_signature() {
        return "S";
    }
};

template <>
struct TypeTraits<jint> : PrimitiveTypeTraits<jint> {
    static std::string get_signature() {
        return "I";
    }
};

template <>
struct TypeTraits<jlong> : PrimitiveTypeTraits<jlong> {
    static std::string get_signature() {
        return "J";
    }
};

template <>
struct TypeTraits<jchar> : PrimitiveTypeTraits<jchar> {
    static std::string get_signature() {
        return "C";
    }
};

template <>
struct TypeTraits<jfloat> : PrimitiveTypeTraits<jfloat> {
    static std::string get_signature() {
        return "F";
    }
};

template <>
struct TypeTraits<jdouble> : PrimitiveTypeTraits<jdouble> {
    static std::string get_signature() {
        return "D";
    }
};

}  // namespace whatjni

#endif  // WHATJNI_TYPE_TRAITS_H
