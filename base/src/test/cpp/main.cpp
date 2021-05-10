#include "gtest/gtest.h"
#include "whatjni/base.h"

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);

    whatjni::load_vm_module(getenv("JVM_LIBRARY_PATH"));

    const jint num_vm_args = 1;
    static const char* vm_args[num_vm_args] = {
        "-Xcheck:jni"
    };
    whatjni::initialize_vm(JNI_VERSION_1_8, num_vm_args, vm_args);

    try {
        return RUN_ALL_TESTS();
    } catch (whatjni::jvm_exception) {
        whatjni::print_exception();
        throw;
    }
}
