#include "java/lang/Throwable.class.h"
#include "java/lang/RuntimeException.class.h"
#include "whatjni/samples/Calculator.class.h"
#include "initialize_classes.h"

#include <iostream>

using whatjni::samples::Calculator;
using namespace whatjni;

int main(int argc, const char** argv) {
    vm_config config(JNI_VERSION_1_8);
    config.extra.push_back("-Xcheck:jni");
    initialize_vm(config);
    initialize_classes();

    try {
        auto calculator = Calculator::new_object();
        auto result = calculator->calculate();
        std::cout << "Result of calculation was " << result << "\n";
    } catch (const jvm_exception& e) {
        std::cout << "Exception " << e.throwable << "\n";
    }
}

jint Calculator::native_staticAdd(jint a, jint b) {
    //throw std::runtime_error("Foo");
    //throw jvm_exception(java::lang::RuntimeException::new_object("Bar"_j));
    return a + b;
}

jint Calculator::native_instanceMultiply(jint a, jint b) {
    return a * b;
}