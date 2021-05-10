#include "gtest/gtest.h"
#include "whatjni/base.h"

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);

    const jint num_vm_args = 1;
    static const char* vm_args[num_vm_args] = {
        "-Xcheck:jni"
    };
    whatjni::initialize_vm(JNI_VERSION_1_8, num_vm_args, vm_args);

    try {
        return RUN_ALL_TESTS();
    } catch (whatjni::JVMException) {
        whatjni::print_exception();
        throw;
    }
}
