#include "whatjni/base.h"
#include "utf8.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#ifdef _WIN32
    #include <windows.h>
    #include <winnt.h>
#else
    #ifndef _GNU_SOURCE
        #define _GNU_SOURCE
    #endif
    #include <dlfcn.h>
    #include <pthread.h>
#endif

namespace whatjni {

using std::string;

#ifdef _WIN32

typedef HMODULE Module;

// Windows 8 and higher
typedef void (WINAPI *GetCurrentThreadStackLimitsFunc)(PULONG_PTR _lowLimit, PULONG_PTR HighLimit);
static GetCurrentThreadStackLimitsFunc GetCurrentThreadStackLimits;

#else  // assume POSIX

typedef void* Module;

#endif

static Module g_vm_module;
static JavaVM* g_vm;
static thread_local JNIEnv* g_env;
static thread_local const char* g_stack_low;
static thread_local size_t g_stack_size;

typedef jint (JNICALL *JNI_CreateJavaVMFunc)(JavaVM **pvm, void **penv, void *args);
static JNI_CreateJavaVMFunc JNI_CreateJavaVM;

static jclass g_object_class;
static jmethodID g_equals_method;
static jmethodID g_hash_code_method;

static jclass g_system_class;
static jmethodID g_identity_hash_code_method;

jvm_error::jvm_error(jint error): error_(error) {
}

jvm_error::jvm_error(const jvm_error& rhs): error_(rhs.error_) {
}

jvm_error::~jvm_error() {
}

jvm_exception::jvm_exception(jobject exception): exception_(exception) {
}

jvm_exception::jvm_exception(const jvm_exception& rhs) {
    exception_ = g_env->NewLocalRef(rhs.exception_);
}

jvm_exception::jvm_exception(jvm_exception&& rhs) {
    exception_ = rhs.exception_;
    rhs.exception_ = nullptr;
}

jvm_exception::~jvm_exception() {
    g_env->DeleteLocalRef(exception_);
}

std::string jvm_exception::get_message() const {
    static jclass clazz = find_class("java/lang/Throwable");
    static jmethodID method = get_method_id(clazz, "getMessage", "()Ljava/lang/String;");
    jstring message = (jstring) call_method<jobject>(exception_, method);
    if (message) {
        jsize length = get_string_length(message);
        jboolean is_copy;
        const char16_t* chars = (const char16_t*) get_string_chars(message, &is_copy);
        std::string result;
        utf8::utf16to8(chars, chars + length, std::back_inserter(result));
        release_string_chars(message, (const jchar*) chars);
        return result;
    } else {
        return "null";
    }
}

static void check_error(int error_code) {
    if (error_code != JNI_OK) {
        throw jvm_error(error_code);
    }
}

static void check_exception() {
    jobject exception = g_env->ExceptionOccurred();
    if (exception) {
        g_env->ExceptionClear();
        throw jvm_exception(exception);
    }
}

template <typename T>
static T check_exception(T result) {
    check_exception();
    return result;
}

static Module open_module(const string& modulePath) {
#ifdef _WIN32
    return LoadLibrary(modulePath.c_str());
#else
    return dlopen(modulePath.c_str(), RTLD_NOW);
#endif
}

static void* lookup_module_symbol(Module module, const char* name) {
#ifdef _WIN32
    return GetProcAddress(module, name);
#else
    return dlsym(module, name);
#endif
}

bool load_vm_module(const char* path) {
    if (g_vm_module) {
        return true;
    }

    if (!path) {
#if defined(_WIN32)
        path = "jvm.dll";
#elif defined(__APPLE__)
        path = "libjvm.dylib";
#else
        path = "libjvm.so";
#endif
    }

    g_vm_module = open_module(path);
    return g_vm_module;
}

static int initialize_inner() {
    if (!load_vm_module(nullptr)) {
        fprintf(stderr, "Could not load JVM module.\n");
        exit(EXIT_FAILURE);
    }

    JNI_CreateJavaVM = (JNI_CreateJavaVMFunc) lookup_module_symbol(g_vm_module, "JNI_CreateJavaVM");

#ifdef _WIN32
    Module kernel32_module = open_module("kernel32.dll");
    GetCurrentThreadStackLimits = (GetCurrentThreadStackLimitsFunc) lookup_module_symbol(kernel32_module,
                                                                                         "GetCurrentThreadStackLimits");
#endif

    return 0;
}

static void initialize() {
    // Since C++11, if multiple threads attempt to initialize the same static local variable concurrently, the
    // initialization occurs exactly once.
    static int result = initialize_inner();
}

void initialize_thread(JNIEnv* env) {
    g_env = env;

#ifdef _WIN32
    if (GetCurrentThreadStackLimits) {
        // Windows 8 and higher.
        const char* stackHigh;
        GetCurrentThreadStackLimits((PULONG_PTR) &g_stack_low, (PULONG_PTR) &stackHigh);
        g_stack_size = stackHigh - g_stack_low;
    } else {
        // No need for GetProcAddress; this is an inline function defined in winnt.h.
        const char** teb = (const char**) NtCurrentTeb();

        // This makes assumptions about the layout of the WIN32 Thread Information Block, which could change. This code
        // will only run on versions prior to Windows 8 though, so such a change seems very unlikely. At the time of
        // writing, this actually still works on NT based Windows up to and including Windows 10 but no reason to use it
        // from Windows 8 onward.
        g_stack_low = teb[2];
        g_stack_size = teb[1] - teb[2];
    }
    
#elif __APPLE__

    pthread_t self = pthread_self();
    g_stack_size = pthread_get_stacksize_np(self);
    g_stack_low = ((const char*) pthread_get_stackaddr_np(self)) - g_stack_size;

#else

    pthread_attr_t attr;
    if (pthread_getattr_np(pthread_self(), &attr) != 0) {
        fprintf(stderr, "pthread_getattr_np failed\n");
        abort();
    }
    
    if (pthread_attr_getstack(&attr, (void**) &g_stack_low, &g_stack_size) != 0) {
        fprintf(stderr, "pthread_attr_getstack failed\n");
        abort();
    }
    
    pthread_attr_destroy(&attr);
#endif  // _WIN32/__APPLE__
}

void initialize_thread() {
    JNIEnv* env = g_env;
    if (!env) {
        check_error(g_vm->GetEnv((void**) &env, JNI_VERSION_1_8));
    }

    initialize_thread(env);
}

void initialize_vm(jint version, jint argc, const char** argv, jboolean ignore_unrecognized) {
    initialize();

    std::vector<JavaVMOption> options(argc);
    for (int i = 0; i < argc; ++i) {
        options[i] = { (char*) argv[i] };
    }

    const JavaVMInitArgs& init_args = {
        version,
        argc,
        options.data(),
        ignore_unrecognized,
    };

    JNIEnv* env;
    check_error(JNI_CreateJavaVM(&g_vm, (void**) &env, (void*) &init_args));
    initialize_thread(env);

    g_object_class = find_class("java/lang/Object");
    g_equals_method = get_method_id(g_object_class, "equals", "(Ljava/lang/Object;)Z");
    g_hash_code_method = get_method_id(g_object_class, "hashCode", "()I");

    g_system_class = find_class("java/lang/System");
    g_identity_hash_code_method = get_static_method_id(g_system_class, "identityHashCode", "(Ljava/lang/Object;)I");
}

void shutdown_vm() {
    check_error(g_vm->DestroyJavaVM());
    g_vm = nullptr;
}

jclass find_class(const char* name) {
    return check_exception(g_env->FindClass(name));
}

jclass get_super_class(jclass clazz) {
    return check_exception(g_env->GetSuperclass(clazz));
}

jboolean is_assignable_from(jclass clazz1, jclass clazz2) {
    auto result = g_env->IsAssignableFrom(clazz1, clazz2);
    check_exception();
    return result;
}

jboolean is_instance_of(jobject obj, jclass clazz) {
    auto result = g_env->IsInstanceOf(obj, clazz);
    check_exception();
    return result;
}

jfieldID get_field_id(jclass clazz, const char* name, const char* sig) {
    return check_exception(g_env->GetFieldID(clazz, name, sig));
}

jfieldID get_static_field_id(jclass clazz, const char* name, const char* sig) {
    return check_exception(g_env->GetStaticFieldID(clazz, name, sig));
}

jmethodID get_method_id(jclass clazz, const char* name, const char* sig) {
    return check_exception(g_env->GetMethodID(clazz, name, sig));
}

jmethodID get_static_method_id(jclass clazz, const char* name, const char* sig) {
    return check_exception(g_env->GetStaticMethodID(clazz, name, sig));
}

jobject alloc_object(jclass clazz) {
    return check_exception(g_env->AllocObject(clazz));
}

jobject new_object(jclass clazz, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    jobject result = g_env->NewObjectV(clazz, method, args);
    va_end(args);
    return check_exception(result);
}

jboolean is_same_object(jobject l, jobject r) {
    auto result = g_env->IsSameObject(l, r);
    check_exception();
    return result;
}

jboolean is_equal_object(jobject l, jobject r) {
    if (l) {
        return call_method<jboolean>(l, g_equals_method, r);
    } else {
        return r == nullptr;
    }
}

jint get_hash_code(jobject obj) {
    if (obj) {
        return call_method<jint>(obj, g_hash_code_method);
    } else {
        return 0;
    }
}

jint get_identity_hash_code(jobject obj) {
    return call_static_method<jint>(g_system_class, g_identity_hash_code_method, obj);
}

jobject new_local_ref(jobject obj) {
    return check_exception(g_env->NewLocalRef(obj));
}

void delete_local_ref(jobject obj) {
    g_env->DeleteLocalRef(obj);
    check_exception();
}

jobject new_global_ref(jobject obj) {
    return check_exception(g_env->NewGlobalRef(obj));
}

void delete_global_ref(jobject obj) {
    g_env->DeleteGlobalRef(obj);
    check_exception();
}

jobject new_weak_global_ref(jobject obj) {
    return check_exception(g_env->NewWeakGlobalRef(obj));
}

void delete_weak_global_ref(jobject obj) {
    g_env->DeleteWeakGlobalRef(obj);
    check_exception();
}

jobjectRefType get_object_ref_type(jobject obj) {
    return check_exception(g_env->GetObjectRefType(obj));
}

static bool is_auto_ref_local(jobject* refref) {
    const char* address = (const char*) refref;
    return address - g_stack_low < g_stack_size;
}

void new_auto_ref(jobject* refref, jobject obj) {
    if (is_auto_ref_local(refref)) {
        *refref = new_local_ref(obj);
    } else {
        *refref = new_global_ref(obj);
    }
}

void move_auto_ref(jobject* to, jobject* from) {
    bool toLocal = is_auto_ref_local(to);
    bool fromLocal = is_auto_ref_local(from);

    if (toLocal == fromLocal) {
        *to = *from;
    } else if (toLocal) {
        *to = new_local_ref(*from);
        delete_global_ref(*from);
    } else {
        *to = new_global_ref(*from);
        delete_local_ref(*from);
    }

    *from = nullptr;
}

void delete_auto_ref(jobject* refref) {
    if (is_auto_ref_local(refref)) {
        delete_local_ref(*refref);
    } else {
        delete_global_ref(*refref);
    }
}

void print_exception() {
    g_env->ExceptionDescribe();
}


template <>
void set_field(jobject obj, jfieldID field, jboolean value) {
    g_env->SetBooleanField(obj, field, value);
    check_exception();
}

template<>
jboolean get_field(jobject obj, jfieldID field) {
    return check_exception(g_env->GetBooleanField(obj, field));
}


template <>
void set_field(jobject obj, jfieldID field, jbyte value) {
    g_env->SetByteField(obj, field, value);
    check_exception();
}


template<>
jbyte get_field(jobject obj, jfieldID field) {
    return check_exception(g_env->GetByteField(obj, field));
}


template <>
void set_field(jobject obj, jfieldID field, jshort value) {
    g_env->SetShortField(obj, field, value);
    check_exception();
}

template<>
jshort get_field(jobject obj, jfieldID field) {
    return check_exception(g_env->GetShortField(obj, field));
}


template <>
void set_field(jobject obj, jfieldID field, jint value) {
    g_env->SetIntField(obj, field, value);
    check_exception();
}

template<>
jint get_field(jobject obj, jfieldID field) {
    return check_exception(g_env->GetIntField(obj, field));
}


template <>
void set_field(jobject obj, jfieldID field, jlong value) {
    g_env->SetLongField(obj, field, value);
    check_exception();
}

template<>
jlong get_field(jobject obj, jfieldID field) {
    return check_exception(g_env->GetLongField(obj, field));
}

template <>
void set_field(jobject obj, jfieldID field, jchar value) {
    g_env->SetCharField(obj, field, value);
    check_exception();
}
template<>
jchar get_field(jobject obj, jfieldID field) {
    return check_exception(g_env->GetCharField(obj, field));
}


template <>
void set_field(jobject obj, jfieldID field, jfloat value) {
    g_env->SetFloatField(obj, field, value);
    check_exception();
}

template<>
jfloat get_field(jobject obj, jfieldID field) {
    return check_exception(g_env->GetFloatField(obj, field));
}


template <>
void set_field(jobject obj, jfieldID field, jdouble value) {
    g_env->SetDoubleField(obj, field, value);
    check_exception();
}

template<>
jdouble get_field(jobject obj, jfieldID field) {
    return check_exception(g_env->GetDoubleField(obj, field));
}


template <>
void set_field(jobject obj, jfieldID field, jobject value) {
    g_env->SetObjectField(obj, field, value);
    check_exception();
}

template<>
jobject get_field(jobject obj, jfieldID field) {
    return check_exception(g_env->GetObjectField(obj, field));
}


template<>
jbyte get_static_field(jclass clazz, jfieldID field) {
    return check_exception(g_env->GetStaticByteField(clazz, field));
}


template <>
void set_static_field(jclass clazz, jfieldID field, jshort value) {
    g_env->SetStaticShortField(clazz, field, value);
    check_exception();
}

template<>
jshort get_static_field(jclass clazz, jfieldID field) {
    return check_exception(g_env->GetStaticShortField(clazz, field));
}


template <>
void set_static_field(jclass clazz, jfieldID field, jint value) {
    g_env->SetStaticIntField(clazz, field, value);
    check_exception();
}

template<>
jint get_static_field(jclass clazz, jfieldID field) {
    return check_exception(g_env->GetStaticIntField(clazz, field));
}


template <>
void set_static_field(jclass clazz, jfieldID field, jlong value) {
    g_env->SetStaticLongField(clazz, field, value);
    check_exception();
}

template<>
jlong get_static_field(jclass clazz, jfieldID field) {
    return check_exception(g_env->GetStaticLongField(clazz, field));
}

template <>
void set_static_field(jclass clazz, jfieldID field, jchar value) {
    g_env->SetStaticCharField(clazz, field, value);
    check_exception();
}
template<>
jchar get_static_field(jclass clazz, jfieldID field) {
    return check_exception(g_env->GetStaticCharField(clazz, field));
}


template <>
void set_static_field(jclass clazz, jfieldID field, jfloat value) {
    g_env->SetStaticFloatField(clazz, field, value);
    check_exception();
}

template<>
jfloat get_static_field(jclass clazz, jfieldID field) {
    return check_exception(g_env->GetStaticFloatField(clazz, field));
}


template <>
void set_static_field(jclass clazz, jfieldID field, jdouble value) {
    g_env->SetStaticDoubleField(clazz, field, value);
    check_exception();
}

template<>
jdouble get_static_field(jclass clazz, jfieldID field) {
    return check_exception(g_env->GetStaticDoubleField(clazz, field));
}


template <>
void set_static_field(jclass clazz, jfieldID field, jobject value) {
    g_env->SetStaticObjectField(clazz, field, value);
    check_exception();
}

template<>
jobject get_static_field(jclass clazz, jfieldID field) {
    return check_exception(g_env->GetStaticObjectField(clazz, field));
}


template <>
void call_method(jobject obj, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    g_env->CallVoidMethodV(obj, method, args);
    va_end(args);
    check_exception();
}

template <>
jboolean call_method(jobject obj, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallBooleanMethodV(obj, method, args);
    va_end(args);
    return check_exception(result);
}

template <>
jbyte call_method(jobject obj, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallByteMethodV(obj, method, args);
    va_end(args);
    return check_exception(result);
}

template <>
jshort call_method(jobject obj, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallShortMethodV(obj, method, args);
    va_end(args);
    return check_exception(result);
}

template <>
jint call_method(jobject obj, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallIntMethodV(obj, method, args);
    va_end(args);
    return check_exception(result);
}

template <>
jlong call_method(jobject obj, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallLongMethodV(obj, method, args);
    va_end(args);
    return check_exception(result);
}

template <>
jchar call_method(jobject obj, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallCharMethodV(obj, method, args);
    va_end(args);
    return check_exception(result);
}

template <>
jfloat call_method(jobject obj, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallFloatMethodV(obj, method, args);
    va_end(args);
    return check_exception(result);
}

template <>
jdouble call_method(jobject obj, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallDoubleMethodV(obj, method, args);
    va_end(args);
    return check_exception(result);
}

template <>
jobject call_method(jobject obj, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallObjectMethodV(obj, method, args);
    va_end(args);
    return check_exception(result);
}


template <>
void call_nonvirtual_method(jobject obj, jclass clazz, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    g_env->CallNonvirtualVoidMethodV(obj, clazz, method, args);
    va_end(args);
    check_exception();
}

template <>
jboolean call_nonvirtual_method(jobject obj, jclass clazz, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallNonvirtualBooleanMethodV(obj, clazz, method, args);
    va_end(args);
    return check_exception(result);
}

template <>
jbyte call_nonvirtual_method(jobject obj, jclass clazz, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallNonvirtualByteMethodV(obj, clazz, method, args);
    va_end(args);
    return check_exception(result);
}

template <>
jshort call_nonvirtual_method(jobject obj, jclass clazz, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallNonvirtualShortMethodV(obj, clazz, method, args);
    va_end(args);
    return check_exception(result);
}

template <>
jint call_nonvirtual_method(jobject obj, jclass clazz, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallNonvirtualIntMethodV(obj, clazz, method, args);
    va_end(args);
    return check_exception(result);
}

template <>
jlong call_nonvirtual_method(jobject obj, jclass clazz, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallNonvirtualLongMethodV(obj, clazz, method, args);
    va_end(args);
    return check_exception(result);
}

template <>
jchar call_nonvirtual_method(jobject obj, jclass clazz, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallNonvirtualCharMethodV(obj, clazz, method, args);
    va_end(args);
    return check_exception(result);
}

template <>
jfloat call_nonvirtual_method(jobject obj, jclass clazz, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallNonvirtualFloatMethodV(obj, clazz, method, args);
    va_end(args);
    return check_exception(result);
}

template <>
jdouble call_nonvirtual_method(jobject obj, jclass clazz, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallNonvirtualDoubleMethodV(obj, clazz, method, args);
    va_end(args);
    return check_exception(result);
}

template <>
jobject call_nonvirtual_method(jobject obj, jclass clazz, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallNonvirtualObjectMethodV(obj, clazz, method, args);
    va_end(args);
    return check_exception(result);
}


template <>
void call_static_method(jclass clazz, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    g_env->CallStaticVoidMethodV(clazz, method, args);
    va_end(args);
    check_exception();
}

template <>
jboolean call_static_method(jclass clazz, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallStaticBooleanMethodV(clazz, method, args);
    va_end(args);
    return check_exception(result);
}

template <>
jbyte call_static_method(jclass clazz, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallStaticByteMethodV(clazz, method, args);
    va_end(args);
    return check_exception(result);
}

template <>
jshort call_static_method(jclass clazz, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallStaticShortMethodV(clazz, method, args);
    va_end(args);
    return check_exception(result);
}

template <>
jint call_static_method(jclass clazz, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallStaticIntMethodV(clazz, method, args);
    va_end(args);
    return check_exception(result);
}

template <>
jlong call_static_method(jclass clazz, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallStaticLongMethodV(clazz, method, args);
    va_end(args);
    return check_exception(result);
}

template <>
jchar call_static_method(jclass clazz, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallStaticCharMethodV(clazz, method, args);
    va_end(args);
    return check_exception(result);
}

template <>
jfloat call_static_method(jclass clazz, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallStaticFloatMethodV(clazz, method, args);
    va_end(args);
    return check_exception(result);
}

template <>
jdouble call_static_method(jclass clazz, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallStaticDoubleMethodV(clazz, method, args);
    va_end(args);
    return check_exception(result);
}

template <>
jobject call_static_method(jclass clazz, jmethodID method, ...) {
    va_list args;
    va_start(args, method);
    auto result = g_env->CallStaticObjectMethodV(clazz, method, args);
    va_end(args);
    return check_exception(result);
}

jstring new_string(const jchar* str, jsize length) {
    return check_exception(g_env->NewString(str, length));
}

jstring new_utf8_string(const char* str, jsize length) {
    bool fastPath = true;
    for (size_t i = 0; i < length; ++i) {
        char c = str[i];
        if (unsigned char(c - 1) >= 0xEF) {  // null character or first byte of a 4 byte character
            fastPath = false;
            break;
        }
    }

    if (fastPath) {
        return g_env->NewStringUTF(str);
    } else {
        // The JNI "modified UTF-8" encoding differs from UTF-8 in its representation of null characters and characters
        // requiring a 4-byte sequence in UTF-8. In that case, transcode to a temporary UTF-16 string before passing
        // to JNI API.
        // https://docs.oracle.com/javase/7/docs/technotes/guides/jni/spec/types.html#wp16542
        std::u16string str16;
        str16.reserve(length);
        utf8::utf8to16(str, str + length, std::back_inserter(str16));
        return g_env->NewString((const jchar*) str16.data(), str16.length());
    }
}

jstring new_utf8_string(const char* str) {
    jsize length = 0;
    bool fastPath = true;
    for (size_t i = 0;; ++i) {
        char c = str[i];
        if (c == 0) {
            length = i;
            break;
        } else if (unsigned char(c) >= 0xF0) {
            fastPath = false;
        }
    }

    if (fastPath) {
        // The JNI "modified UTF-8" encoding differs from UTF-8 in its representation of null characters and characters
        // requiring a 4-byte sequence in UTF-8. In that case, transcode to a temporary UTF-16 string before passing
        // to JNI API.
        // https://docs.oracle.com/javase/7/docs/technotes/guides/jni/spec/types.html#wp16542
        return g_env->NewStringUTF(str);
    } else {
        std::u16string str16;
        str16.reserve(length);
        utf8::utf8to16(str, str + length, std::back_inserter(str16));
        return g_env->NewString((const jchar*) str16.data(), str16.length());
    }
}


jsize get_string_length(jstring str) {
    return check_exception(g_env->GetStringLength(str));
}

const jchar* get_string_chars(jstring str, jboolean* is_copy) {
    return check_exception(g_env->GetStringChars(str, is_copy));
}

void release_string_chars(jstring str, const jchar* chars) {
    g_env->ReleaseStringChars(str, chars);
    check_exception();
}

template <>
jarray new_primitive_array<jboolean>(jsize size) {
    return check_exception(g_env->NewBooleanArray(size));
}

template <>
jarray new_primitive_array<jbyte>(jsize size) {
    return check_exception(g_env->NewByteArray(size));
}

template <>
jarray new_primitive_array<jshort>(jsize size) {
    return check_exception(g_env->NewShortArray(size));
}

template <>
jarray new_primitive_array<jint>(jsize size) {
    return check_exception(g_env->NewIntArray(size));
}

template <>
jarray new_primitive_array<jlong>(jsize size) {
    return check_exception(g_env->NewLongArray(size));
}

template <>
jarray new_primitive_array<jchar>(jsize size) {
    return check_exception(g_env->NewCharArray(size));
}

template <>
jarray new_primitive_array<jfloat>(jsize size) {
    return check_exception(g_env->NewFloatArray(size));
}

template <>
jarray new_primitive_array<jdouble>(jsize size) {
    return check_exception(g_env->NewDoubleArray(size));
}

jarray new_object_array(jsize size, jclass element_class, jobject initial_element) {
    return check_exception(g_env->NewObjectArray(size, element_class, initial_element));
}

jsize get_array_length(jarray array) {
    return check_exception(g_env->GetArrayLength(array));
}


template <>
jboolean get_array_element(jarray array, jsize idx) {
    jboolean value;
    g_env->GetBooleanArrayRegion((jbooleanArray) array, idx, 1, &value);
    check_exception();
    return value;
}

template <>
void set_array_element(jarray array, jsize idx, jboolean value) {
    g_env->SetBooleanArrayRegion((jbooleanArray) array, idx, 1, &value);
    check_exception();
}

template<>
jboolean* get_array_elements(jarray array, jboolean* is_copy) {
    return check_exception(g_env->GetBooleanArrayElements((jbooleanArray) array, is_copy));
}

template <> void release_array_elements(jarray array, jboolean* elements, jint mode) {
    g_env->ReleaseBooleanArrayElements((jbooleanArray) array, elements, mode);
    check_exception();
}

template <>
jbyte get_array_element(jarray array, jsize idx) {
    jbyte value;
    g_env->GetByteArrayRegion((jbyteArray) array, idx, 1, &value);
    check_exception();
    return value;
}

template <>
void set_array_element(jarray array, jsize idx, jbyte value) {
    g_env->SetByteArrayRegion((jbyteArray) array, idx, 1, &value);
    check_exception();
}

template<>
jbyte* get_array_elements(jarray array, jboolean* is_copy) {
    return check_exception(g_env->GetByteArrayElements((jbyteArray) array, is_copy));
}

template <> void release_array_elements(jarray array, jbyte* elements, jint mode) {
    g_env->ReleaseByteArrayElements((jbyteArray) array, elements, mode);
    check_exception();
}

template <>
jshort get_array_element(jarray array, jsize idx) {
    jshort value;
    g_env->GetShortArrayRegion((jshortArray) array, idx, 1, &value);
    check_exception();
    return value;
}

template <>
void set_array_element(jarray array, jsize idx, jshort value) {
    g_env->SetShortArrayRegion((jshortArray) array, idx, 1, &value);
    check_exception();
}

template<>
jshort* get_array_elements(jarray array, jboolean* is_copy) {
    return check_exception(g_env->GetShortArrayElements((jshortArray) array, is_copy));
}

template <> void release_array_elements(jarray array, jshort* elements, jint mode) {
    g_env->ReleaseShortArrayElements((jshortArray) array, elements, mode);
    check_exception();
}

template <>
jint get_array_element(jarray array, jsize idx) {
    jint value;
    g_env->GetIntArrayRegion((jintArray) array, idx, 1, &value);
    check_exception();
    return value;
}

template <>
void set_array_element(jarray array, jsize idx, jint value) {
    g_env->SetIntArrayRegion((jintArray) array, idx, 1, &value);
    check_exception();
}

template<>
jint* get_array_elements(jarray array, jboolean* is_copy) {
    return check_exception(g_env->GetIntArrayElements((jintArray) array, is_copy));
}

template <> void release_array_elements(jarray array, jint* elements, jint mode) {
    g_env->ReleaseIntArrayElements((jintArray) array, elements, mode);
    check_exception();
}

template <>
jlong get_array_element(jarray array, jsize idx) {
    jlong value;
    g_env->GetLongArrayRegion((jlongArray) array, idx, 1, &value);
    check_exception();
    return value;
}

template <>
void set_array_element(jarray array, jsize idx, jlong value) {
    g_env->SetLongArrayRegion((jlongArray) array, idx, 1, &value);
    check_exception();
}

template<>
jlong* get_array_elements(jarray array, jboolean* is_copy) {
    return check_exception(g_env->GetLongArrayElements((jlongArray) array, is_copy));
}

template <> void release_array_elements(jarray array, jlong* elements, jint mode) {
    g_env->ReleaseLongArrayElements((jlongArray) array, elements, mode);
    check_exception();
}

template <>
jchar get_array_element(jarray array, jsize idx) {
    jchar value;
    g_env->GetCharArrayRegion((jcharArray) array, idx, 1, &value);
    check_exception();
    return value;
}

template <>
void set_array_element(jarray array, jsize idx, jchar value) {
    g_env->SetCharArrayRegion((jcharArray) array, idx, 1, &value);
    check_exception();
}

template<>
jchar* get_array_elements(jarray array, jboolean* is_copy) {
    return check_exception(g_env->GetCharArrayElements((jcharArray) array, is_copy));
}

template <> void release_array_elements(jarray array, jchar* elements, jint mode) {
    g_env->ReleaseCharArrayElements((jcharArray) array, elements, mode);
    check_exception();
}

template <>
jfloat get_array_element(jarray array, jsize idx) {
    jfloat value;
    g_env->GetFloatArrayRegion((jfloatArray) array, idx, 1, &value);
    check_exception();
    return value;
}

template <>
void set_array_element(jarray array, jsize idx, jfloat value) {
    g_env->SetFloatArrayRegion((jfloatArray) array, idx, 1, &value);
    check_exception();
}

template<>
jfloat* get_array_elements(jarray array, jboolean* is_copy) {
    return check_exception(g_env->GetFloatArrayElements((jfloatArray) array, is_copy));
}

template <> void release_array_elements(jarray array, jfloat* elements, jint mode) {
    g_env->ReleaseFloatArrayElements((jfloatArray) array, elements, mode);
    check_exception();
}

template <>
jdouble get_array_element(jarray array, jsize idx) {
    jdouble value;
    g_env->GetDoubleArrayRegion((jdoubleArray) array, idx, 1, &value);
    check_exception();
    return value;
}

template <>
void set_array_element(jarray array, jsize idx, jdouble value) {
    g_env->SetDoubleArrayRegion((jdoubleArray) array, idx, 1, &value);
    check_exception();
}

template<>
jdouble* get_array_elements(jarray array, jboolean* is_copy) {
    return check_exception(g_env->GetDoubleArrayElements((jdoubleArray) array, is_copy));
}

template <> void release_array_elements(jarray array, jdouble* elements, jint mode) {
    g_env->ReleaseDoubleArrayElements((jdoubleArray) array, elements, mode);
    check_exception();
}

template <>
jobject get_array_element(jarray array, jsize idx) {
    return check_exception(g_env->GetObjectArrayElement((jobjectArray) array, idx));
}

template <>
void set_array_element(jarray array, jsize idx, jobject value) {
    g_env->SetObjectArrayElement((jobjectArray) array, idx, value);
    check_exception();
}

void* get_primitive_array_critical(jarray array, jboolean* is_copy) {
    void* ptr = g_env->GetPrimitiveArrayCritical(array, is_copy);

    // Must not call check_exception, ExceptionOccurred or any other JNI function before releasing aside from nesting
    // more getPrimitiveArrayCritical/releasePrimitiveArrayCritical pairs.
    if (!ptr) {
        throw jvm_error(JNI_ERR);
    }

    return ptr;
}

void release_primitive_array_critical(jarray array, void* elements, jint mode) {
    g_env->ReleasePrimitiveArrayCritical(array, elements, mode);
}

void push_local_frame(jint capacity) {
    g_env->PushLocalFrame(capacity);
    check_exception();
}

jobject pop_local_frame(jobject result) {
    return check_exception(g_env->PopLocalFrame(result));
}

}  // namespace whatjni
