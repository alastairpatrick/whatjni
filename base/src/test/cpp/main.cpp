#include "gtest/gtest.h"
#include "whatjni/base.h"

using namespace whatjni;

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);

    vm_config config(JNI_VERSION_1_8);
    config.extra.push_back("-Xcheck:jni");

    initialize_vm(config);

    try {
        return RUN_ALL_TESTS();
    } catch (whatjni::jvm_exception) {
        whatjni::print_exception();
        throw;
    }
}
