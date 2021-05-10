#ifndef WHATJNI_BASE_H
#define WHATJNI_BASE_H

#include "whatjni/jni.h"

#ifdef _MSC_VER
#pragma warning(disable: 4584)  // 'java::lang::String': base-class 'java::lang::Object' is already a base-class of 'java::io::Serializable'
#endif

#ifndef WHATJNI_LANG
    // https://devblogs.microsoft.com/cppblog/msvc-now-correctly-reports-__cplusplus/
    #ifdef _MSC_VER
        #define WHATJNI_LANG _MSVC_LANG
    #else
        #define WHATJNI_LANG __cplusplus
    #endif
#endif

#ifndef WHATJNI_BASE
    #ifdef _WIN32
        #ifdef WHATJNI_BASE_EXPORT
            #define WHATJNI_BASE __declspec(dllexport)
        #else
            #define WHATJNI_BASE __declspec(dllimport)
        #endif
    #else
        #define WHATJNI_BASE
    #endif
#endif  // ifndef WHATJNI_BASE

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


namespace whatjni {

// For errors returned by Java Invocation API.
class WHATJNI_BASE jvm_error {
    jint error_;
public:
    explicit jvm_error(jint error);
    jvm_error(const jvm_error&);
    ~jvm_error();

    jint error() { return error_; }

    jvm_error& operator=(const jvm_error&) = delete;
};

// Thrown when JVM exception detected.
class WHATJNI_BASE jvm_exception {
public:
    jvm_exception();
    jvm_exception(const jvm_exception&);
    ~jvm_exception();

    jvm_exception& operator=(const jvm_exception&) = delete;
};

WHATJNI_BASE bool load_vm_module(const char* path);
WHATJNI_BASE void initialize_vm(jint version, jint argc, const char** argv, jboolean ignore_unrecognized = JNI_FALSE);
WHATJNI_BASE void shutdown_vm();

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
WHATJNI_BASE void delete_global_ref(jobject obj);
WHATJNI_BASE jobject new_weak_global_ref(jobject obj);
WHATJNI_BASE void delete_weak_global_ref(jobject obj);
WHATJNI_BASE jobjectRefType get_object_ref_type(jobject obj);

WHATJNI_BASE void new_auto_ref(jobject* refref, jobject obj);
WHATJNI_BASE void move_auto_ref(jobject* to, jobject* from);
WHATJNI_BASE void delete_auto_ref(jobject* refref);

WHATJNI_BASE void throw_new_exception(jclass clazz, const char* message);
WHATJNI_BASE jobject current_exception();
WHATJNI_BASE void print_exception();
WHATJNI_BASE void clear_exception();


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

WHATJNI_BASE jstring new_string(const jchar* str, jsize length);
WHATJNI_BASE jsize get_string_length(jstring str);
WHATJNI_BASE const jchar* get_string_chars(jstring str, jboolean* is_copy);
WHATJNI_BASE void release_string_chars(jstring str, const jchar* chars);

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

}  // namespace whatjni

#undef WHATJNI_EACH_PRIMITIVE_TYPE
#undef WHATJNI_EACH_JAVA_TYPE

#endif  // WHATJNI_BASE_H
