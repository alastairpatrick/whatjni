#include "gtest/gtest.h"
#include "whatjni/base.h"

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);

    const jint num_vm_args = 3;
    static const char* vm_args[num_vm_args] = {
        "-Xms1024M",
        "-Xmx2048M",  // otherwise CI tests fail on MacOS with OpenJ9 VM with JVMJ9VM015W Initialization error for library j9gc29(2): Failed to instantiate compressed references metadata.  200M requested
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
