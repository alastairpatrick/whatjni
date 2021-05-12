#include "whatjni/samples/Calculator.class.h"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using whatjni::samples::Calculator;
using namespace whatjni;

int main(int argc, const char** argv) {
    whatjni::load_vm_module(getenv("JVM_LIBRARY_PATH"));

    std::string defineClassPath = std::string("-Djava.class.path=") + getenv("CLASSPATH");
    std::vector<const char*> vm_args(argv + 1, argv + argc);
    vm_args.push_back(defineClassPath.data());

    whatjni::initialize_vm(JNI_VERSION_1_8, vm_args.size(), vm_args.data());

    try {
        Calculator::register_natives();

        auto calculator = Calculator::new_object();
        auto result = calculator->calculate();
        std:: cout << "Result of calculation was " << result << "\n";
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