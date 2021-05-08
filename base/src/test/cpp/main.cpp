#include "gtest/gtest.h"
#include "whatjni/base.h"

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    whatjni::initialize_vm(JNI_VERSION_1_8, argc - 1, argv + 1);

    try {
        return RUN_ALL_TESTS();
    } catch (whatjni::JVMException) {
        whatjni::print_exception();
        throw;
    }
}
