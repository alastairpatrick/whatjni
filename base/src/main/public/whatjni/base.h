#ifndef WHATJNI_BASE_H
#define WHATJNI_BASE_H

#include "whatjni/jni.h"

#include <string>
#include <vector>

#ifndef WHATJNI_LANG
    // VC++ is not compliant with C++ spec in setting the __cplusplus macro.
    // https://devblogs.microsoft.com/cppblog/msvc-now-correctly-reports-__cplusplus/
    #ifdef _MSC_VER
        #define WHATJNI_LANG _MSVC_LANG
    #else
        #define WHATJNI_LANG __cplusplus
    #endif
#endif

#ifndef WHATJNI_EMPTY_BASES
    // C++11 requires that the empty base class optimization be applied to standard layout types. VC++ is not
    // compliant with the spec in this regard.
    // https://devblogs.microsoft.com/cppblog/optimizing-the-layout-of-empty-base-classes-in-vs2015-update-2-3/
    #ifdef _MSC_VER
        #define WHATJNI_EMPTY_BASES __declspec(empty_bases)
    #else
        #define WHATJNI_EMPTY_BASES
    #endif
#endif

#if !defined(WHATJNI_BASE) && defined(_WIN32) && defined(WHATJNI_BASE_EXPORT)
    #define WHATJNI_BASE __declspec(dllexport)
#endif  // ifndef WHATJNI_BASE

#ifndef WHATJNI_BASE
    #define WHATJNI_BASE
#endif

#ifndef WHATJNI_IF_PROPERTY
    #if defined(_MSC_VER)
        #define WHATJNI_IF_PROPERTY(x) x
    #else
        #if defined(__has_declspec_attribute)
            #if __has_declspec_attribute(property)
                #define WHATJNI_IF_PROPERTY(x) x
            #else
                #define WHATJNI_IF_PROPERTY(x)
            #endif
        #else
            #define WHATJNI_IF_PROPERTY(x)
        #endif
    #endif
#endif  // WHATJNI_IF_PROPERTY

#define WHATJNI_EACH_PRIMITIVE_TYPE() \
X(jboolean)                           \
X(jbyte)                              \
X(jshort)                             \
X(jint)                               \
X(jlong)                              \
X(jchar)                              \
X(jfloat)                             \
X(jdouble)                            \

#define WHATJNI_EACH_JAVA_TYPE()      \
WHATJNI_EACH_PRIMITIVE_TYPE()         \
X(jobject)                            \


namespace java {
namespace lang {
class Throwable;
}  // namespace lang
}  // namespace java

namespace whatjni {

// For errors returned by Java Invocation API.
class WHATJNI_BASE jvm_error {
public:
    const jint error;

    explicit jvm_error(jint error);
    jvm_error(const jvm_error&);
    ~jvm_error();

    jvm_error& operator=(const jvm_error&) = delete;
};
static_assert(std::is_standard_layout<jvm_error>::value);

// Thrown when JVM exception detected.
class WHATJNI_BASE jvm_exception {
public:
    java::lang::Throwable* const throwable;

    explicit jvm_exception(jobject exception);
    explicit jvm_exception(java::lang::Throwable*);
    jvm_exception(const jvm_exception& rhs);
    ~jvm_exception();

    jvm_exception& operator=(const jvm_exception&) = delete;

    void schedule() const;
};
static_assert(std::is_standard_layout<jvm_exception>::value);

struct vm_config {
    explicit vm_config(jint version): version(version) {}

    jint version = 0;
    std::string vm_module_path;  // path to jvm.dll / libjvm.so
    std::vector<std::string> classpath;
    jboolean ignore_unrecognized = false;
    std::vector<std::string> extra;
};

WHATJNI_BASE void initialize_vm(const vm_config& config);
WHATJNI_BASE void shutdown_vm();

WHATJNI_BASE void initialize_thread(JNIEnv* env);

WHATJNI_BASE jclass find_class(const char* name);
WHATJNI_BASE jclass get_super_class(jclass clazz);
WHATJNI_BASE jboolean is_assignable_from(jclass clazz1, jclass clazz2);
WHATJNI_BASE jboolean is_instance_of(jobject obj, jclass clazz);
WHATJNI_BASE jfieldID get_field_id(jclass clazz, const char* name, const char* sig);
WHATJNI_BASE jfieldID get_static_field_id(jclass clazz, const char* name, const char* sig);
WHATJNI_BASE jmethodID get_method_id(jclass clazz, const char* name, const char* sig);
WHATJNI_BASE jmethodID get_static_method_id(jclass clazz, const char* name, const char* sig);

WHATJNI_BASE jobject alloc_object(jclass clazz);
WHATJNI_BASE jobject new_object(jclass clazz, jmethodID method, ...);

WHATJNI_BASE jboolean is_same_object(jobject l, jobject r);
WHATJNI_BASE jboolean is_equal_object(jobject l, jobject r);
WHATJNI_BASE jint get_hash_code(jobject obj);
WHATJNI_BASE jint get_identity_hash_code(jobject obj);

WHATJNI_BASE jobject new_local_ref(jobject obj);
WHATJNI_BASE void delete_local_ref(jobject obj);
WHATJNI_BASE jobject new_global_ref(jobject obj);
WHATJNI_BASE jobject new_global_ref_then_delete_local_ref(jobject obj);
WHATJNI_BASE void delete_global_ref(jobject obj);
WHATJNI_BASE jobject new_weak_global_ref(jobject obj);
WHATJNI_BASE void delete_weak_global_ref(jobject obj);
WHATJNI_BASE jobjectRefType get_object_ref_type(jobject obj);

WHATJNI_BASE void schedule_new_runtime_exception(const char* message);
WHATJNI_BASE void print_exception();


template <typename T>
void set_field(jobject obj, jfieldID field, T value);

#define X(T) template<> WHATJNI_BASE void set_field(jobject obj, jfieldID field, T value);
WHATJNI_EACH_JAVA_TYPE()
#undef X


template <typename T>
T get_field(jobject obj, jfieldID field);

#define X(T) template<> WHATJNI_BASE T get_field(jobject obj, jfieldID field);
WHATJNI_EACH_JAVA_TYPE()
#undef X


template <typename T>
void set_static_field(jclass clazz, jfieldID field, T value);

#define X(T) template<> void WHATJNI_BASE set_static_field(jclass jclazz, jfieldID field, T value);
WHATJNI_EACH_JAVA_TYPE()
#undef X


template <typename T>
T get_static_field(jclass clazz, jfieldID field);

#define X(T) template<> WHATJNI_BASE T get_static_field(jclass clazz, jfieldID field);
WHATJNI_EACH_JAVA_TYPE()
#undef X


template <typename T>
T call_method(jobject obj, jmethodID method, ...);

template <>
void WHATJNI_BASE call_method(jobject obj, jmethodID method, ...);

#define X(T) template<> WHATJNI_BASE T call_method(jobject obj, jmethodID method, ...);
WHATJNI_EACH_JAVA_TYPE()
#undef X


template <typename T>
T call_nonvirtual_method(jobject obj, jclass clazz, jmethodID method, ...);

template <>
WHATJNI_BASE void call_nonvirtual_method(jobject obj, jclass clazz, jmethodID method, ...);

#define X(T) template<> WHATJNI_BASE T call_nonvirtual_method(jobject obj, jclass clazz, jmethodID method, ...);
WHATJNI_EACH_JAVA_TYPE()
#undef X


template <typename T>
T call_static_method(jclass clazz, jmethodID method, ...);

template <>
WHATJNI_BASE void call_static_method(jclass clazz, jmethodID method, ...);

#define X(T) template<> WHATJNI_BASE T call_static_method(jclass clazz, jmethodID method, ...);
WHATJNI_EACH_JAVA_TYPE()
#undef X

// JNI has two ways of representing string data to native code:
//  1) a multibyte representation referred to as "UTF" that does not correspond to any UTF standard
//  2) actual UTF-16
//
// Yes, it's backwards!
//
// The proprietary multibyte encoding is generally not UTF-8. It uses the same representation as UTF-8 for strings
// that do not contain null characters or characters outside the Basic Multilingual Plane. Otherwise it is different
// from UTF-8. See the section "Modified UTF-8 Strings" in the JNI specification.
//
// https://docs.oracle.com/javase/7/docs/technotes/guides/jni/spec/types.html#wp16542
//
// It is appropriate to use char to represent multibyte characters, since char does not imply any particular character
// encoding. We use char16_t to represent UTF-16 characters, since char16_t is specifically for UTF-16.
WHATJNI_BASE jstring new_string(const char16_t* str, jsize length);
WHATJNI_BASE jstring new_string(const char* str);
WHATJNI_BASE jsize get_string_length(jstring str);
WHATJNI_BASE jsize get_string_multibyte_size(jstring str);
WHATJNI_BASE const char16_t* get_string_utf16_chars_critical(jstring str, jboolean* is_copy);
WHATJNI_BASE void release_string_utf16_chars_critical(jstring str, const char16_t* chars);
WHATJNI_BASE const char* get_string_multibyte_chars(jstring str, jboolean* is_copy);
WHATJNI_BASE void release_string_multibyte_chars(jstring str, const char* chars);
WHATJNI_BASE void get_string_region(jstring str, jsize start, jsize length, char16_t* chars);
WHATJNI_BASE void get_string_region(jstring str, jsize start, jsize length, char* chars);
WHATJNI_BASE jstring to_string(jobject obj);

template <typename T> jarray new_primitive_array(jsize size);

#define X(T) template<> WHATJNI_BASE jarray new_primitive_array<T>(jsize size);
WHATJNI_EACH_PRIMITIVE_TYPE()
#undef X

WHATJNI_BASE jarray new_object_array(jsize size, jclass element_class, jobject initial_element);

WHATJNI_BASE jsize get_array_length(jarray array);

template <typename T> T get_array_element(jarray array, jsize idx);

#define X(T) template<> WHATJNI_BASE T get_array_element(jarray array, jsize idx);
WHATJNI_EACH_JAVA_TYPE()
#undef X

template <typename T> void set_array_element(jarray array, jsize idx, T value);

#define X(T) template<> WHATJNI_BASE void set_array_element(jarray array, jsize idx, T value);
WHATJNI_EACH_JAVA_TYPE()
#undef X

template <typename T> T* get_array_elements(jarray array, jboolean* isCopy);

#define X(T) template<> WHATJNI_BASE T* get_array_elements(jarray array, jboolean* isCopy);
WHATJNI_EACH_PRIMITIVE_TYPE()
#undef X

template <typename T> void release_array_elements(jarray array, T* elements, jint mode);

#define X(T) template<> WHATJNI_BASE void release_array_elements(jarray array, T* elements, jint mode);
WHATJNI_EACH_PRIMITIVE_TYPE()
#undef X

WHATJNI_BASE void* get_primitive_array_critical(jarray array, jboolean* isCopy);
WHATJNI_BASE void release_primitive_array_critical(jarray array, void* elements, jint mode);

WHATJNI_BASE void push_local_frame(jint size);
WHATJNI_BASE jobject pop_local_frame(jobject result = nullptr);

WHATJNI_BASE void register_natives(jclass clazz, const JNINativeMethod* methods, jint numMethods);

}  // namespace whatjni

#undef WHATJNI_EACH_PRIMITIVE_TYPE
#undef WHATJNI_EACH_JAVA_TYPE

#endif  // WHATJNI_BASE_H
