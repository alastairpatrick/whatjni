#include "whatjni/samples/Calculator.class.h"
#include "initialize_classes.h"

#include <iostream>

using whatjni::samples::Calculator;
using namespace whatjni;

int main(int argc, const char** argv) {
    initialize_vm(vm_config(JNI_VERSION_1_8));
    initialize_classes();

    try {
        auto calculator = Calculator::new_object();
        auto result = calculator->calculate();
        std::cout << "Result of calculation was " << result << "\n";
    } catch (const jvm_exception& e) {
        std::cout << "Exception " << e.get_message() << "\n";
    }
}

jint Calculator::native_staticAdd(jint a, jint b) {
    return a + b;
}

jint Calculator::native_instanceMultiply(jint a, jint b) {
    return a * b;
}